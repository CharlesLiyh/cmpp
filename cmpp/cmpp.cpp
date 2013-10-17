#include "stdafx.h"
#include <cstdint>
#include <memory>
#include "cmpp.h"
#include "communicator.h" 
#include "SocketStream.h"
#include "connect.h"
#include "terminate.h"
#include "submit.h"
#include "delivery.h"
#include "active.h"

using namespace std;
using namespace comm;

namespace cmpp {
	struct DeliveryAcceptors {
		SMSAcceptor smsAcceptor;
		ReportAcceptor reportAcceptor;
	};

	MessageGateway::MessageGateway(const char* endpoint, int port, SMSAcceptor sAcceptor, ReportAcceptor rAcceptor, long lives, float timeoutVal, float interval) {
		maxLives = lives;
		timeout = timeoutVal;
		heartbeatInterval = interval;

		acceptors = new DeliveryAcceptors;
		acceptors->smsAcceptor = sAcceptor;
		acceptors->reportAcceptor = rAcceptor;

		stream = new SocketStream(endpoint, port);
		communicator = new Communicator(*stream);
		spid = nullptr;
	}

	MessageGateway::~MessageGateway() {
		delete acceptors;
		delete communicator;
		delete stream;
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
				// arrival action 这个action会在接收到Delivery包时执行
				[this, delivery, deliveryRes](bool, const string&) {
					deliveryRes->setMsgId(delivery->messageId());
					delivery->dispatch(acceptors->smsAcceptor, acceptors->reportAcceptor);
			},
				// departure action, 下面的action会在deliveryRes发送完成后执行
				[delivery, deliveryRes](bool, const string&) {
					delete delivery;
					delete deliveryRes;
			}
			);
		});
	}

	void MessageGateway::registerActiveHandler() {
		communicator->registerPassiveArrivalHandler(Active::CommandId, [this]()->tuple<Arrival*, const Departure*, Communicator::ResponseAction, Communicator::ResponseAction> {
			Active* active = new Active;
			ActiveEcho* activeEcho = new ActiveEcho;
			return make_tuple(active, activeEcho, 
				// arrival action 这个action会在接收到Delivery包时执行
				[this, active, activeEcho](bool, const string&) {
#pragma message(__TODO__"refresh last active time")
			},
				// departure action, 下面的action会在deliveryRes发送完成后执行
				[active, activeEcho](bool, const string&) {
					delete active;
					delete activeEcho;
			}
			);
		});
	}

	void MessageGateway::keepActive() {
		long lives = maxLives;
		const DWORD interval = (DWORD)(heartbeatInterval * 1000.0);
		static Active beat;
		static ActiveEcho echo;
		while(lives>0) {
			DWORD eventIdx = ::WaitForSingleObject(heartbeatEvent, interval);
			if (eventIdx==WAIT_TIMEOUT) {
				long remainLives = InterlockedDecrement(&lives);

				if (remainLives>0) {
					communicator->exchange(&beat, &echo,[this, &lives](bool, const string&) {
						// 一旦收到心跳回应包，则重置心跳状态，恢复‘生命活力’
						::InterlockedExchange(&lives, maxLives);
					});
				}
			}
			else {
				eventIdx -= WAIT_OBJECT_0;
			}
		}
	}

	unsigned long __stdcall MessageGateway::heartbeatThread( void* self ) {
		((MessageGateway*)self)->keepActive();
		return 0;
	}
}
