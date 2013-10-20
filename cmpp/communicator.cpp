#include "stdafx.h"
#include "communicator.h"
#include "serialization.h"
#include "socketstream.h"
#include "AccessLock.h"
#include <utility>
#include <algorithm>
#include <cstdlib>
#include <string>
using namespace std;

#pragma comment(lib, "ws2_32.lib")

namespace comm {
	class UnhandledRetcodeFromWaitForXXXFunctions {
	public:
		UnhandledRetcodeFromWaitForXXXFunctions(unsigned aCode) {
			code = aCode;
		}

	private:
		unsigned code;
	};

	//////////////////////////////////////////////////////////////////////////
	// Communicator

	Communicator::Communicator(const char* endpoint, int port, size_t sendingWndSize, int lives, double timeoutVal)
		:stream(endpoint, port) 
	{
		maxLives = lives;
		timeout = timeoutVal;
		retryTimers = new HANDLE[sendingWndSize];
		timersCount = sendingWndSize;
	}

	Communicator::~Communicator() {
		delete retryTimers;
	}
		
	static HANDLE debugEvent = ::CreateEventA(NULL, FALSE, FALSE, "comm.debugEvent");

	bool Communicator::open(ErrorReporter aReporter) {
		sequenceId = 0;
		stopAllStuffs = false;
		errorAlreadyReported = false;
		errorReporter = aReporter;

		if (stream.open()) {
			HANDLE* ptrTimers = (HANDLE*)retryTimers;
			for (size_t i=0; i<timersCount; ++i) {
				HANDLE freshTime = ::CreateWaitableTimer(NULL, FALSE, NULL);
				ptrTimers[i] = freshTime;
				freeTimers.push_back(freshTime);
			}

			activeDepartSem = ::CreateSemaphore(NULL, 0, 65535, NULL);
			passiveDepartSem = ::CreateSemaphore(NULL, 0, 65535, NULL);
			timersSem = ::CreateSemaphore(NULL, timersCount, timersCount, NULL);
			shootingSem = ::CreateSemaphore(NULL, 0, timersCount, NULL);
			shootWaitingThread = ::CreateThread(NULL, 0, waitForShootingThread, this, 0, NULL);
			sendThread = ::CreateThread(NULL, 0, sendingThreadFunc, this, 0, NULL);
			recvThread = ::CreateThread(NULL, 0, receivingThreadFunc, this, 0, NULL);
			
			return true;
		}

		return false;
	}

	void Communicator::loopCombineShootingEvent() {
		HANDLE events[] = {timersSem, activeDepartSem};
		while(!stopAllStuffs) {
			DWORD code = ::WaitForMultipleObjectsEx(_countof(events), events, TRUE, INFINITE, TRUE);

			if (WAIT_FAILED==code || WAIT_IO_COMPLETION==code)
				break;
			
			::ReleaseSemaphore(shootingSem, 1, NULL);
		}
	}

	DWORD Communicator::waitForShootingThread(LPVOID self) {
		((Communicator*)self)->loopCombineShootingEvent();
		return 0;
	}

	void Communicator::stopStuffs() {
		stopAllStuffs = true;
		::QueueUserAPC(interruptWaiting, sendThread, 0);
		::QueueUserAPC(interruptWaiting, shootWaitingThread, 0);
		stream.close();
	}

	void Communicator::close() {
		stopStuffs();

		// 等待数据发送线程‘寿终正寝’
		::WaitForSingleObject(sendThread, INFINITE);
		::WaitForSingleObject(recvThread, INFINITE);
		::WaitForSingleObject(shootWaitingThread, INFINITE);

		HANDLE* ptr2timers = (HANDLE*)retryTimers;
		for (size_t i=0; i<timersCount; ++i) {
			::CloseHandle(ptr2timers[i]);
		}
		// 使用swap技巧彻底移除freeTimers中的所有内容
		freeTimers.swap(list<HANDLE>());

		::CloseHandle(timersSem);
		::CloseHandle(shootingSem);
		::CloseHandle(activeDepartSem);
		::CloseHandle(passiveDepartSem);
		::CloseHandle(sendThread);
		::CloseHandle(recvThread);
		::CloseHandle(shootWaitingThread);
	}

	pair<const uint8_t*,size_t> Communicator::buildPayload(const Departure& departure, uint32_t seqId) {
		uint8_t buffer[1024] = {0};

		StreamWriter writer(buffer);
		writer<<departure.commandId()<<seqId;
		departure.serialize(writer);

		size_t payloadSize = writer.bytesWritten();

		uint8_t* payload = new uint8_t[payloadSize];
		memcpy_s(payload, payloadSize, buffer, payloadSize);

		return make_pair(payload, payloadSize);
	}

	void Communicator::exchange(const Departure* departure, Arrival* arrival, ResponseAction action) {
		// 如果请求发送的行为发生在错误已经发生的情况下，则应直接忽略该请求，并告知失败
		if (errorAlreadyReported) {
			action(false, "");
			return;
		}

		// 在将departure和arrival入队前，首先获取它们对应的departureId
		// 考虑到exchange的异步访问可能，使用效率最高的InterlockedIncrement进行递增
		uint32_t departureId = InterlockedIncrement(&sequenceId);

		departuresLock.lock([this, &departureId, departure, arrival, &action](){
			// 由于是异步发送，所以这里只是将待发送的departure对象以及对应的arrival回应
			// 放到相应的队列中，而不是直接发送。
			// 此时，由于并未真正发送，所以也就不需要处理任何定时器相关的事宜

			seqid2arrival.insert(make_pair(departureId, make_pair(arrival, action)));
			activeDepartures.push_back(make_tuple(departureId, departure, [](bool, const string&){}));
		});

		::ReleaseSemaphore(activeDepartSem, 1, NULL);

		dump("exchange");
	}

	void Communicator::registerPassiveArrivalHandler(uint32_t cmdId, GivenArrivalHandler handler) {
		givenHandlers.insert(make_pair(cmdId, handler));
	}

	bool Communicator::send(tuple<uint32_t, const Departure*, ResponseAction>& departTuple) {
		uint32_t departId = get<0>(departTuple);
		const Departure* departure = get<1>(departTuple);
		ResponseAction action = get<2>(departTuple);

		auto payloadPair = buildPayload(*departure, departId);
		auto_ptr<const uint8_t> payload(payloadPair.first);
		size_t payloadSize = payloadPair.second;

		return stream.send(payload.get(), payloadSize);
	}

	bool Communicator::sendPassiveDeparture() {
		tuple<uint32_t, const Departure*, ResponseAction> departTuple;
		passiveDepartLock.lock([this, &departTuple]{
			departTuple = passiveDepartures.front();
			passiveDepartures.pop_front();
		});

		// 这里只需要对服务器所发送的请求包进行响应，不需要验证该响应
		// 是否被服务器所接受，所以无需任何定时器的处理
		return send(departTuple);
	}

	void Communicator::activeDepartureTimer(HANDLE timer) {
		LARGE_INTEGER firstTrigger;
		firstTrigger.QuadPart = -(long long)(timeout * 10000000);
		::SetWaitableTimer(timer, &firstTrigger, (long)(timeout*1000.0), NULL, NULL, FALSE);
	}

	bool Communicator::sendActiveDeparture() {
		// 当有待发送的用户请求，且有定时器可用时（发送窗口），触发一次主动请求的发送行为
		tuple<uint32_t, const Departure*, ResponseAction> departTuple;
		HANDLE timer;
		departuresLock.lock([this, &departTuple, &timer]{
			departTuple = activeDepartures.front();
			activeDepartures.pop_front();

			timer = freeTimers.front();
			freeTimers.pop_front();

			uint32_t departId = get<0>(departTuple);
			const Departure* departure = get<1>(departTuple);

			// 下面立刻会执行一次发送任务，所以设置的初始剩余发送次数为mavLives - 1
			seqid2retryStuffs.insert(make_pair(departId, make_pair(timer, maxLives -1)));
			timer2departure.insert(make_pair(timer, departTuple));
		});

		if (send(departTuple)) {
			activeDepartureTimer(timer);
			return true;
		}

		return false;
	}

	void Communicator::reportError( const char* message ) {
		if (!errorAlreadyReported) {
			errorAlreadyReported = true;
			errorReporter(message);
		}
	}
	
	void Communicator::loopSending() {
		const size_t countOfEvents = 2+timersCount;
		HANDLE* events = new HANDLE[countOfEvents];

		// 无论是因为异常还是函数返回，都应该安全地释放events内存资源
		// auto_ptr技巧可以很好地做到这一点
		auto_ptr<HANDLE> eventsGuard(events);

		events[0] = shootingSem;
		events[1] = passiveDepartSem;
		size_t sizeofTimers = sizeof(HANDLE)*timersCount;
		memcpy_s(events+2, sizeofTimers, retryTimers, sizeofTimers);

		enum { ShootDeparture = WAIT_OBJECT_0 + 0, PassiveDeparture, FirstTimeoutObject};

		bool socketWorkable = true;
		static auto isShooting = [](DWORD eventIdx){ return eventIdx==ShootDeparture; };
		static auto isPassiveDepart = [](DWORD eventIdx) { return eventIdx==PassiveDeparture; };
		static auto errorOccurs = [](DWORD eventIdx) { return eventIdx<WAIT_OBJECT_0 || (eventIdx>=MAXIMUM_WAIT_OBJECTS && eventIdx!=WAIT_IO_COMPLETION); };
		auto isTimeout = [this](DWORD eventIdx) { return eventIdx>=FirstTimeoutObject && eventIdx<=FirstTimeoutObject + timersCount; };

		bool hasDeadDeparture = false;
		while (socketWorkable && !stopAllStuffs && !hasDeadDeparture) {
			DWORD eventIdx = ::WaitForMultipleObjectsEx(countOfEvents, events, FALSE, INFINITE,TRUE);
			if (isShooting(eventIdx)) {
				socketWorkable = sendActiveDeparture();
			} else if (isPassiveDepart(eventIdx)) {
				socketWorkable = sendPassiveDeparture();
			} else if (isTimeout(eventIdx)) {
				HANDLE timer = retryTimers[eventIdx-WAIT_OBJECT_0-2];
				int remainLives;

				tuple<uint32_t, const Departure*, ResponseAction> departTuple;
				departuresLock.lock([this, &departTuple, timer, &remainLives](){
					departTuple = timer2departure[timer];

					uint32_t departId = get<0>(departTuple);
					pair<HANDLE, int>& timerStuff = seqid2retryStuffs[departId];
					remainLives = timerStuff.second;
					timerStuff.second -= 1;					
				});

				hasDeadDeparture = remainLives <= 0;
				if (remainLives>0 && send(departTuple)) {
					activeDepartureTimer(timer);
				}
			} else if (errorOccurs(eventIdx)){
				throw UnhandledRetcodeFromWaitForXXXFunctions(eventIdx); 
			}

			dump("sending");

			// else 还有一种可能性：WAIT_IO_COMPLETION，对此，不做任何处理即可，因为APC调用
			// 的目的只是为了让WaitForMultipleObjectsEx有机会强制返回，并进行一次while的判断
		}

		if (hasDeadDeparture) {
			reportError("数据发送超时");
		} else if (!socketWorkable) {
			reportError("数据发送失败");
		}

		static auto notifySendingFailure = [](tuple<uint32_t, const Departure*, ResponseAction>& departTuple) {
			ResponseAction& action = get<2>(departTuple);
			action(false, "");
		};

		// 对于所有仍然没有发送成功的任务，执行它们对应的响应action，使得它们有机会释放
		// 对应的资源
		for_each(activeDepartures.begin(), activeDepartures.end(), notifySendingFailure);
		for_each(passiveDepartures.begin(), passiveDepartures.end(), notifySendingFailure);

		if (!stopAllStuffs) {
			stopStuffs();
		}
	}

	void Communicator::loopReceiving() {
		static auto isEcho = [this](uint32_t commandId) { 
			return givenHandlers.find(commandId)==givenHandlers.end();
		};

		bool socketWorkable = true;
		while(socketWorkable && !stopAllStuffs) {
			socketWorkable = stream.recv([this](const uint8_t* payload, size_t payloadSize){
				StreamReader reader(payload);
				uint32_t commandId, arrivalId;
				reader>>commandId>>arrivalId;

				payload = payload + sizeof(commandId) + sizeof(arrivalId);

				if (isEcho(commandId)) {
					handleArrivalResponse(arrivalId, payload);
				} else {
					handleArrivalRequest(commandId, payload, arrivalId);
				}			
			});
			
			dump("receiving");
		}

		if (!stopAllStuffs) {
			// 由于网络异常导致的发送终止，而不是用户的主动取消
			// 此时需停止数据发送工作
			stopStuffs();
			reportError("数据接收失败");
		}
	}

	DWORD Communicator::sendingThreadFunc(LPVOID self) {
		((Communicator*)self)->loopSending();
		return 0;
	}

	DWORD Communicator::receivingThreadFunc(LPVOID self) {
		((Communicator*)self)->loopReceiving();
		return 0;
	}

	void __stdcall Communicator::interruptWaiting( unsigned long self ) {
		// 这里什么都不用做，此函数存在的意义只是为了让WaitForSingleObjectEx、WaitForMultipleObjectsEx能够有机会从阻塞状态
		// 返回，然后检查循环标记并终止循环。
	}

	void Communicator::handleArrivalResponse( uint32_t arrivalId, const uint8_t* payload ) {
		pair<Arrival*, ResponseAction> responsePair;
		HANDLE timer;
		bool isDeprecated = false;
		departuresLock.lock([this, &responsePair, arrivalId, &isDeprecated, &timer](){
			bool hasCorrespondingArrivalProcessor = seqid2arrival.find(arrivalId)!=seqid2arrival.end();
			bool hasCorrespondingTimer = seqid2retryStuffs.find(arrivalId)!=seqid2retryStuffs.end();
			isDeprecated = !hasCorrespondingArrivalProcessor || !hasCorrespondingTimer;
			if (!isDeprecated) {
				responsePair = seqid2arrival.at(arrivalId);
				seqid2arrival.erase(arrivalId);

				auto timerStuffs = seqid2retryStuffs[arrivalId];
				timer = timerStuffs.first;
				seqid2retryStuffs.erase(arrivalId);

				timer2departure.erase(timer);

				::CancelWaitableTimer(timer);
				freeTimers.push_back(timer);
				::ReleaseSemaphore(timersSem, 1, NULL);
			}
		});

		if (!isDeprecated) {
			Arrival* arrival = responsePair.first;
			ResponseAction action = responsePair.second;
			StreamReader payloadReader(payload );
			arrival->deserialize(payloadReader);
			action(true, "");
		}
		// else 如果找不到对应的departure则认为是重复的数据包，直接丢弃即可
	}

	void Communicator::handleArrivalRequest( uint32_t commandId, const uint8_t* payload, uint32_t arrivalId ) {
		GivenArrivalHandler& handler = givenHandlers.at(commandId);
		auto counterParts = handler();
		Arrival* specialResponse = get<0>(counterParts);
		const Departure* departure = get<1>(counterParts);
		ResponseAction& arrivalAction = get<2>(counterParts);
		ResponseAction& departureAction = get<3>(counterParts);

		StreamReader reader(payload);
		specialResponse->deserialize(reader);
		arrivalAction(true, "");

		passiveDepartLock.lock([this, arrivalId, departure, &departureAction](){
			passiveDepartures.push_back(make_tuple(arrivalId, departure, departureAction));
		});

		::ReleaseSemaphore(passiveDepartSem, 1, NULL);
	}

#ifdef DEBUG_COMM
	unsigned long __stdcall Communicator::dumpThread( void* self ) {
		((Communicator*)self)->dumpLoop();
		return 0;
	}

	void Communicator::dumpLoop() {
		while(!stopAllStuffs) {
			if (WAIT_OBJECT_0==::WaitForSingleObjectEx(debugEvent, INFINITE, TRUE)) {
				dump("");
			}
		}
	}

	extern AccessLock DumpLock;
#endif // DEBUG_COMM

	void Communicator::dump(const char* msg) {
#ifdef DEBUG_COMM
		printf("------------------dump:%s-------------------\n", msg);
		printf("SEQID:%d 待发送数据包:%d\n", sequenceId, activeDepartures.size());
		printf("seqid2arrival:%d\n", seqid2arrival.size());
		printf("activeDepartures:%d\n", activeDepartures.size());
		printf("passiveDepartures:%d\n", passiveDepartures.size());
		printf("freeTimes:%d\n", freeTimers.size());
		printf("timer2departure:%d\n", timer2departure.size());
		printf("seqid2timer:%d\n", seqid2retryStuffs.size());
		printf("-----------------------------------------\n");
#endif // DEBUG_COMM
	}
}