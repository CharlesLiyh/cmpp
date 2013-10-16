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

using namespace std;
using namespace comm;

namespace cmpp {
	struct DeliveryAcceptors {
		SMSAcceptor smsAcceptor;
		ReportAcceptor reportAcceptor;
	};

	MessageGateway::MessageGateway(const char* endpoint, int port, SMSAcceptor sAcceptor, ReportAcceptor rAcceptor) {
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

		Connect* request = new Connect(account, 1380813704);
		ConnectResponse* response = new ConnectResponse();
		communicator->exchange(request, response, [this, account, request, response, action](bool success, const string& failure){
			action(response->isVerified(), response->errorMessage().c_str());

			delete request;
			delete response;
		});
	}

	void MessageGateway::close() {
		Terminate* request = new Terminate();
		TerminateResponse* response = new TerminateResponse();

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
}
