#include "stdafx.h"
#include "communicator.h"
#include "serialization.h"
#include "AccessLock.h"
#include <utility>
using namespace std;

#pragma comment(lib, "ws2_32.lib")

namespace comm {

	//////////////////////////////////////////////////////////////////////////
	// Communicator

	Communicator::Communicator(const char* anEndpoint, int aPort){
		endpoint = _strdup(anEndpoint);
		port = aPort;
		seqid2arrival = new map< uint32_t, pair<Arrival*, ResponseAction> >;
		givenHandlers = new map<uint32_t, GivenArrivalHandler>;
		departures = new list< tuple<uint32_t, const Departure*, ResponseAction> >;

		closeEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		exchangeLock = new AccessLock();
	}

	Communicator::~Communicator() {
		delete departures;
		delete givenHandlers;
		delete seqid2arrival;
		delete endpoint;
		delete exchangeLock;
		::CloseHandle(closeEvent);
	}

	bool Communicator::open() {
		sequenceId = 0;
		if (!setupSocket())
			return false;

		::ResetEvent(closeEvent);
		departuresSemaphore = ::CreateSemaphore(NULL, 0, 65535, NULL);

		DWORD threadId;
		sendThread = ::CreateThread(NULL, 0, sendingThreadFunc, this, 0, &threadId);
		recvThread = ::CreateThread(NULL, 0, receivingThreadFunc, this, 0, &threadId);
		return true;
	}

	void Communicator::close() {
		::SetEvent(closeEvent);
		closesocket(tcpSocket);

		// 等待数据发送线程‘寿终正寝’
		::WaitForSingleObject(sendThread, INFINITE);
		::WaitForSingleObject(recvThread, INFINITE);

		::CloseHandle(departuresSemaphore);
	}

	pair<const uint8_t*,size_t> Communicator::buildPayload(const Departure& departure, uint32_t seqId) {
		uint8_t buffer[1024] = {0};

		const uint32_t HeaderSize = sizeof(uint32_t);

		StreamWriter payloadWriter(buffer+HeaderSize);
		payloadWriter<<departure.commandId()<<seqId;
		departure.serialize(payloadWriter);

		uint32_t payloadSize = HeaderSize + (uint32_t)payloadWriter.bytesWritten();

		StreamWriter headerWriter(buffer);
		headerWriter<<payloadSize;

		uint8_t* payload = new uint8_t[payloadSize];
		memcpy_s(payload, payloadSize, buffer, payloadSize);

		return make_pair(payload, payloadSize);
	}

	void Communicator::exchange(const Departure* departure, Arrival* arrival, ResponseAction action) {
		uint32_t departureId;
		exchangeLock->lock([this, &departureId, departure, arrival, &action](){
			departureId = sequenceId++;
			seqid2arrival->insert(make_pair(departureId, make_pair(arrival, action)));
			departures->push_back(make_tuple(departureId, departure, [](bool, const string&){}));
		});

		::ReleaseSemaphore(departuresSemaphore, 1, NULL);
	}

	void Communicator::registerPassiveArrivalHandler(uint32_t cmdId, GivenArrivalHandler handler) {
		givenHandlers->insert(make_pair(cmdId, handler));
	}
	
	void Communicator::handleSending() {
		bool moreJobs = true;
		while (moreJobs) {
			HANDLE events[] = { closeEvent, departuresSemaphore };
			DWORD eventIdx = ::WaitForMultipleObjects(sizeof(events)/sizeof(events[0]), events, FALSE, INFINITE);
			eventIdx -= WAIT_OBJECT_0;
			if (eventIdx==0) {
				moreJobs = false;
			} else {
				tuple<uint32_t, const Departure*, ResponseAction> departTuple;
				exchangeLock->lock([this, &departTuple]{
					// 既然信号量被释放，意味着departures中一定有待发送的Departure
					departTuple = departures->front();
					departures->pop_front();
				});
				uint32_t departId = get<0>(departTuple);
				const Departure* departure = get<1>(departTuple);
				ResponseAction departureAction = get<2>(departTuple);

				auto payloadPair = buildPayload(*departure, departId);
				const uint8_t* payload = payloadPair.first;
				size_t payloadSize = payloadPair.second;

				sendTrunk(payload, payloadSize);

				delete payload;
				departureAction(true, "");
			}
		}
	}

	void Communicator::handleReceiving() {
		bool success = true;
		while(success) {
			int32_t sizeWrap;
			success = success && receiveTrunk((uint8_t*)&sizeWrap, sizeof(sizeWrap));
			if (!success)
				break;

			size_t payloadSize = ntohl(sizeWrap);
			payloadSize -= sizeof(sizeWrap);

			auto_ptr<uint8_t> buff(new uint8_t[payloadSize]);

			success = success && receiveTrunk(buff.get(), payloadSize);
			if (!success)
				break;

			printf("income package, size:%d\n", payloadSize);
			for (size_t i=0; i<payloadSize; ++i) {
				printf("%02X ", buff.get()[i]);
				if ( ((i+1)%16)==0 ) {
					printf("\n");
				}
			}
			printf( payloadSize%16==0 ? "\n" : "\n\n");

			StreamReader reader(buff.get());

			uint32_t commandId, arrivalId;
			reader>>commandId>>arrivalId;

			bool isPassiveArrival = givenHandlers->find(commandId)!=givenHandlers->end();
			if (isPassiveArrival) {
				GivenArrivalHandler& handler = givenHandlers->at(commandId);
				auto counterParts = handler();
				Arrival* specialResponse = get<0>(counterParts);
				const Departure* departure = get<1>(counterParts);
				ResponseAction& arrivalAction = get<2>(counterParts);
				ResponseAction& departureAction = get<3>(counterParts);

				specialResponse->deserialize(reader);
				arrivalAction(true, "");

				exchangeLock->lock([this, arrivalId, departure, &departureAction](){
					departures->push_back(make_tuple(arrivalId, departure, departureAction));
				});
				::ReleaseSemaphore(departuresSemaphore, 1, NULL);
			} else {
				pair<Arrival*, ResponseAction> responsePair;
				exchangeLock->lock([this, &responsePair, arrivalId](){
					responsePair = seqid2arrival->at(arrivalId);
					seqid2arrival->erase(arrivalId);
				});

				Arrival* arrival = responsePair.first;
				ResponseAction action = responsePair.second;
				arrival->deserialize(reader);
				action(true, "");
			}
		}
	}

	DWORD Communicator::sendingThreadFunc(LPVOID self) {
		((Communicator*)self)->handleSending();
		return 0;
	}

	DWORD Communicator::receivingThreadFunc(LPVOID self) {
		((Communicator*)self)->handleReceiving();
		return 0;
	}

	bool Communicator::setupSocket() {
		struct sockaddr_in addr;

		tcpSocket = socket( AF_INET, SOCK_STREAM, 0);

		addr.sin_family = AF_INET;
		addr.sin_port   = htons( port);
		addr.sin_addr.s_addr   = inet_addr(endpoint);

		int errCode = connect(tcpSocket, (struct sockaddr *)&addr, sizeof( addr));
		return errCode==0;
	}

	template<class PayloadType>
	bool process(SOCKET tcpSocket, PayloadType payload, size_t total, int (__stdcall *handler)(SOCKET, PayloadType, int, int)) {
		size_t remain = total;
		bool success = true;
		while(success && remain>0) {
			int handled = handler(tcpSocket, payload + total - remain, remain, 0);
			if (handled>0) {
				remain -= handled;
			} else {
				remain = 0;
				success = false;
			}
		}

		return success;
	}

	bool Communicator::sendTrunk( const uint8_t* payload, size_t total ) {
		return process(tcpSocket, (const char*)payload, total, &::send);
	}

	bool Communicator::receiveTrunk( uint8_t* buff, size_t total ) {
		return process(tcpSocket, (char*)buff, total, &::recv);
	}
}