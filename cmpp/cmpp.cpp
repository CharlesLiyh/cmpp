#include "stdafx.h"
#include <cstdint>
#include <memory>
#include "cmpp.h"
#include "communicator.h" 
#include "connect.h"
#include "terminate.h"
#include "submit.h"
#include "delivery.h"
#include "active.h"
#include "cancel.h"
#include "Protocol20.h"
#include "query.h"

using namespace std;
using namespace comm;

namespace cmpp {
	// Active 和 ActiveEcho没有任何有意义的数据，所以整个库都可以直接使用这两个静态对象
	// 来进行数据的整编和解编，以减少内存的申请和释放行为，提高运行效率
	static Active sharedActive;
	static ActiveEcho sharedActiveEcho;

	struct DeliveryAcceptors {
		SMSAcceptor smsAcceptor;
		ReportAcceptor reportAcceptor;
		ErrorReporter errorReporter;
	};

	MessageGateway::MessageGateway(Protocol20* proto, SMSAcceptor sAcceptor, ReportAcceptor rAcceptor, ErrorReporter errorReporter, float interval) {
		heartbeatInterval = interval;
		spid = nullptr;

		protocol = proto;
		communicator = proto->createCommunicator();
		acceptors = new DeliveryAcceptors;
		acceptors->smsAcceptor = sAcceptor;
		acceptors->reportAcceptor = rAcceptor;
		acceptors->errorReporter = errorReporter;
	}

	MessageGateway::~MessageGateway() {
		delete acceptors;
		protocol->releaseObject(communicator);
	}

	bool MessageGateway::open(const SPID* account, CommonAction action) {
		spid = _strdup(account->username());

		auto stopHeartbeatWhenErrorOccurs = [this](const string& message){
			acceptors->errorReporter(message.c_str());
			stopHeartbeat();

			// 即使communicator发生了异常，MessageGateway也不应该对自身执行close
			// 因为这样会导致线程对自身调用‘结束等待’过程，近而造成死锁。
		};

		if (communicator->open(stopHeartbeatWhenErrorOccurs)) {
			registerDeliveriesHandler(); 
			registerActiveHandler();
			authenticate(account, action);

			heartbeatStopEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
			heartbeatThreadHandle = ::CreateThread(NULL, 0, heartbeatThread, this, 0, NULL);
			return true;
		}

		return false;
	}

	void MessageGateway::close() {
		stopHeartbeat();
		::WaitForSingleObject(heartbeatThreadHandle, INFINITE);

		// close必须是同步的，原因如下：
		// 如果MessageGateway的内部是单线程的，那么close一定是同步的，无须解释
		// 如果MessageGateway的处理使用了多线程，考虑到一个线程不可能等待自己结束，因为这样会
		// 造成死循环。所以内部线程的结束一定是由MessageGateway所使用的线程之外的线程（例如主线程）
		// 来监控的。这意味着，close对于调用者来说，一定是阻塞的。
		HANDLE doneEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);

		// Terminate数据包没有任何有意义的数据，即使多线程环境下也可以同时访问，所以定义为静态
		// 对象即可
		static Terminate request;
		static TerminateResponse response;

		communicator->exchange(&request, &response, [this, doneEvent](bool) {
			::SetEvent(doneEvent);
		});

		// 仅当Terminate的请求被处理完成时（有可能处理失败，当连接首先断开的情形下，但无论哪种情况，
		// 该请求必须被执行）
		::WaitForSingleObject(doneEvent, INFINITE);

		communicator->close();
		delete spid;
	}

	void MessageGateway::send( const MessageTask* task, SubmitAction action ) {
		SubmitSMS* request = new SubmitSMS(task, spid);
		SubmitSMSResponse* response = new SubmitSMSResponse;

		communicator->exchange(request, response, [request, response, action](bool success) {
			if (success)
				action(response->isSuccess(), response->reason().c_str(), response->messageId());

			delete request;
			delete response;
		});
	}

	void MessageGateway::cancel( uint64_t taskId, SimpleAction action ) {
		Cancel *request = new Cancel(taskId);
		CancelResponse* response = new CancelResponse;

		communicator->exchange(request, response, [request, response, action](bool success) {
			if (success)
				action(response->isSucceed());

			delete request;
			delete response;
		});
	}

	void MessageGateway::authenticate( const SPID* account, CommonAction& action ) {
		Connect* request = new Connect(account, 1380813704);
		ConnectResponse* response = new ConnectResponse();
		communicator->exchange(request, response, [this, account, request, response, action](bool success){
			// success 为 false时，意味着无法处理登录请求，所以不进行数据通告
			if (success)
				action(response->isVerified(), response->errorMessage().c_str());

			delete request;
			delete response;
		});
	}

	void MessageGateway::registerDeliveriesHandler() {
		communicator->registerPassiveArrivalHandler(Delivery::CommandId, [this]()->tuple<Arrival*, const Departure*, Communicator::ResponseAction, Communicator::ResponseAction> {
			Delivery* delivery = new Delivery;
			DeliveryResponse* deliveryRes = new DeliveryResponse;
			return make_tuple(delivery, deliveryRes, 
				// arrival action 在接收到Delivery包时执行
				[this, delivery, deliveryRes](bool success) {
					// success 为 false时，意味着无法处理登录请求，所以不进行数据通告
					if (success) {
						deliveryRes->setMsgId(delivery->messageId());
						delivery->dispatch(acceptors->smsAcceptor, acceptors->reportAcceptor);
					}
			},
				// departure action, 在deliveryRes发送完成后执行
				[delivery, deliveryRes](bool) {
					delete delivery;
					delete deliveryRes;
			}
			);
		});
	}

	void MessageGateway::registerActiveHandler() {
		communicator->registerPassiveArrivalHandler(Active::CommandId, [this]()->tuple<Arrival*, const Departure*, Communicator::ResponseAction, Communicator::ResponseAction> {
			return make_tuple(&sharedActive, &sharedActiveEcho, 
				// arrival action，在Active数据包被动接收时执行
				[](bool) {},
				// departure action, 在deliveryRes发送完成后执行
				[](bool) {}
			);
		});
	}

	void MessageGateway::keepActive() {
		bool toKeepAlive = true;
		map< DWORD, function<void()> >actions;
		actions[WAIT_OBJECT_0] = [&toKeepAlive]() {
			toKeepAlive = false;
		};

		actions[WAIT_TIMEOUT] = [this](){
			communicator->exchange(&sharedActive, &sharedActiveEcho,[this](bool) {});
		};

		const DWORD intervalSec = (DWORD)(heartbeatInterval * 1000.0);
		// 当定时器到时，且心跳发送正常时，发送下一个链路检测包
		while( toKeepAlive) {
			 DWORD code = ::WaitForSingleObject(heartbeatStopEvent, intervalSec);
			 actions[code]();
		}

		printf("Heartbeat stopped\n");
	}

	unsigned long __stdcall MessageGateway::heartbeatThread( void* self ) {
		((MessageGateway*)self)->keepActive();
		return 0;
	}

	void MessageGateway::stopHeartbeat() {
		::SetEvent(heartbeatStopEvent);
	}

	void MessageGateway::exchangeQuery(Query* request, QueryResponse* res, Statistics* stat, QueryAction act) {
		communicator->exchange(request, res, [stat, act, res, request](bool success) {
			if (success)
				act(*stat);

			delete res;
			delete stat;
			delete request;
		});
	}

	void MessageGateway::queryForAmounts( uint32_t year, uint8_t month, uint8_t day, QueryAction action ) {
		char dateStr[9];
		sprintf_s(dateStr, "%4d%02d%02d", year, month, day);
		Query* request = new Query(dateStr);
		request->queryForAmounts();

		Statistics* stat = new Statistics;
		QueryResponse* response = new QueryResponse(stat);
		exchangeQuery(request, response, stat, action);
	}

	void MessageGateway::queryForService( uint32_t year, uint8_t month, uint8_t day, const char* serviceCode, QueryAction action ) {
		char dateStr[9];
		sprintf_s(dateStr, "%4d%02d%02d", year, month, day);
		Query* request = new Query(dateStr);
		request->queryForService(serviceCode);

		Statistics* stat = new Statistics;
		QueryResponse* response = new QueryResponse(stat);

		exchangeQuery(request, response, stat, action);
	}
}