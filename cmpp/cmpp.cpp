#include "stdafx.h"
#include "cmpp.h"
#include "communicator.h" 
#include "connect.h"
#include "terminate.h"
#include "submit.h"
#include "delivery.h"
#include "active.h"
#include <cstdint>
#include <memory>

using namespace std;
using namespace comm;

namespace cmpp {
	// Active 和 ActiveEcho没有任何有意义的数据，所以整个库都可以直接使用这两个静态对象
	// 来进行数据的整编和解编，以减少内存的申请和释放行为，提高运行效率
	static Active singleActive;
	static ActiveEcho singleActiveEcho;

	struct DeliveryAcceptors {
		SMSAcceptor smsAcceptor;
		ReportAcceptor reportAcceptor;
	};

	MessageGateway::MessageGateway(Communicator* aCommunicator, SMSAcceptor sAcceptor, ReportAcceptor rAcceptor, float interval) {
		heartbeatInterval = interval;
		spid = nullptr;

		communicator = aCommunicator;
		acceptors = new DeliveryAcceptors;
		acceptors->smsAcceptor = sAcceptor;
		acceptors->reportAcceptor = rAcceptor;
	}

	MessageGateway::~MessageGateway() {
		delete acceptors;
		delete spid;
	}

	void MessageGateway::open(const SPID* account, CommonAction action) {
		spid = _strdup(account->username());
		communicator->open();

		registerDeliveriesHandler(); 
		registerActiveHandler();
		authenticate(account, action);

		heartbeatEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		DWORD pid;
		heartbeatThreadHandle = ::CreateThread(NULL, 0, heartbeatThread, this, 0, &pid);
	}

	void MessageGateway::close() {
		Terminate* request = new Terminate();
		TerminateResponse* response = new TerminateResponse();

		// close必须是同步的，原因如下：
		// 如果MessageGateway的内部是单线程的，那么close一定是同步的，无须解释
		// 如果MessageGateway的处理使用了多线程，考虑到一个线程不可能等待自己结束，因为这样会
		// 造成死循环。所以内部线程的结束一定是由MessageGateway所使用的线程之外的线程（例如主线程）
		// 来监控的。这意味着，close对于调用者来说，一定是阻塞的。
		HANDLE doneEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		communicator->exchange(request, response, [request, response, this, doneEvent](bool, const string& message) {
			delete request;
			delete response;

			::SetEvent(doneEvent);
		});

		::WaitForSingleObject(doneEvent, INFINITE);
		communicator->close();
	}

	void MessageGateway::send( const MessageTask* task, SubmitAction action ) {
		SubmitSMS* request = new SubmitSMS(task, spid);
		SubmitSMSResponse* response = new SubmitSMSResponse;

		communicator->exchange(request, response, [request, response, action](bool, const string& message) {
			action(response->isSuccess(), response->reason().c_str(), response->messageId());

			delete request;
			delete response;
		});
	}

	void MessageGateway::authenticate( const SPID* account, CommonAction& action ) {
		Connect* request = new Connect(account, 1380813704);
		ConnectResponse* response = new ConnectResponse();
		communicator->exchange(request, response, [this, account, request, response, action](bool success, const string& failure){
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
				[this, delivery, deliveryRes](bool, const string&) {
					deliveryRes->setMsgId(delivery->messageId());
					delivery->dispatch(acceptors->smsAcceptor, acceptors->reportAcceptor);
			},
				// departure action, 在deliveryRes发送完成后执行
				[delivery, deliveryRes](bool, const string&) {
					delete delivery;
					delete deliveryRes;
			}
			);
		});
	}

	void MessageGateway::registerActiveHandler() {
		communicator->registerPassiveArrivalHandler(Active::CommandId, [this]()->tuple<Arrival*, const Departure*, Communicator::ResponseAction, Communicator::ResponseAction> {
			return make_tuple(&singleActive, &singleActiveEcho, 
				// arrival action，在Active数据包被动接收时执行
				[this](bool, const string&) {
#pragma message(__TODO__"refresh last active time")
			},
				// departure action, 在deliveryRes发送完成后执行
				[](bool, const string&) {
					// 因为singleActive、singleActiveEcho都是全局的，所以不需要释放
				}
			);
		});
	}

	void MessageGateway::keepActive() {
		function<bool(DWORD)> timeToBeat = [](DWORD eventIdx){ return eventIdx==WAIT_TIMEOUT; };

		const DWORD intervalSec = (DWORD)(heartbeatInterval * 1000.0);
		bool toKeepAlive = true;

		// 当定时器到时，且心跳发送正常时，发送下一个链路检测包
		while( toKeepAlive && timeToBeat(::WaitForSingleObject(heartbeatEvent, intervalSec)) ) {
			communicator->exchange(&singleActive, &singleActiveEcho,[this, &toKeepAlive](bool success, const string&) {
				// 如果心跳发送失败，则置toKeepAlive为false，停止心跳循环
				toKeepAlive = success;
			});
		}
	}

	unsigned long __stdcall MessageGateway::heartbeatThread( void* self ) {
		((MessageGateway*)self)->keepActive();
		return 0;
	}
}