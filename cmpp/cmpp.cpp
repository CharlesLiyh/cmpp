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
	// Active �� ActiveEchoû���κ�����������ݣ����������ⶼ����ֱ��ʹ����������̬����
	// ���������ݵ�����ͽ�࣬�Լ����ڴ��������ͷ���Ϊ���������Ч��
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

		// close������ͬ���ģ�ԭ�����£�
		// ���MessageGateway���ڲ��ǵ��̵߳ģ���ôcloseһ����ͬ���ģ��������
		// ���MessageGateway�Ĵ���ʹ���˶��̣߳����ǵ�һ���̲߳����ܵȴ��Լ���������Ϊ������
		// �����ѭ���������ڲ��̵߳Ľ���һ������MessageGateway��ʹ�õ��߳�֮����̣߳��������̣߳�
		// ����صġ�����ζ�ţ�close���ڵ�������˵��һ���������ġ�
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
				// arrival action �ڽ��յ�Delivery��ʱִ��
				[this, delivery, deliveryRes](bool, const string&) {
					deliveryRes->setMsgId(delivery->messageId());
					delivery->dispatch(acceptors->smsAcceptor, acceptors->reportAcceptor);
			},
				// departure action, ��deliveryRes������ɺ�ִ��
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
				// arrival action����Active���ݰ���������ʱִ��
				[this](bool, const string&) {
#pragma message(__TODO__"refresh last active time")
			},
				// departure action, ��deliveryRes������ɺ�ִ��
				[](bool, const string&) {
					// ��ΪsingleActive��singleActiveEcho����ȫ�ֵģ����Բ���Ҫ�ͷ�
				}
			);
		});
	}

	void MessageGateway::keepActive() {
		function<bool(DWORD)> timeToBeat = [](DWORD eventIdx){ return eventIdx==WAIT_TIMEOUT; };

		const DWORD intervalSec = (DWORD)(heartbeatInterval * 1000.0);
		bool toKeepAlive = true;

		// ����ʱ����ʱ����������������ʱ��������һ����·����
		while( toKeepAlive && timeToBeat(::WaitForSingleObject(heartbeatEvent, intervalSec)) ) {
			communicator->exchange(&singleActive, &singleActiveEcho,[this, &toKeepAlive](bool success, const string&) {
				// �����������ʧ�ܣ�����toKeepAliveΪfalse��ֹͣ����ѭ��
				toKeepAlive = success;
			});
		}
	}

	unsigned long __stdcall MessageGateway::heartbeatThread( void* self ) {
		((MessageGateway*)self)->keepActive();
		return 0;
	}
}