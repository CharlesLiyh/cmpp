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
	// Active �� ActiveEchoû���κ�����������ݣ����������ⶼ����ֱ��ʹ����������̬����
	// ���������ݵ�����ͽ�࣬�Լ����ڴ��������ͷ���Ϊ���������Ч��
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

			// ��ʹcommunicator�������쳣��MessageGatewayҲ��Ӧ�ö�����ִ��close
			// ��Ϊ�����ᵼ���̶߳�������á������ȴ������̣��������������
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

		// close������ͬ���ģ�ԭ�����£�
		// ���MessageGateway���ڲ��ǵ��̵߳ģ���ôcloseһ����ͬ���ģ��������
		// ���MessageGateway�Ĵ���ʹ���˶��̣߳����ǵ�һ���̲߳����ܵȴ��Լ���������Ϊ������
		// �����ѭ���������ڲ��̵߳Ľ���һ������MessageGateway��ʹ�õ��߳�֮����̣߳��������̣߳�
		// ����صġ�����ζ�ţ�close���ڵ�������˵��һ���������ġ�
		HANDLE doneEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);

		// Terminate���ݰ�û���κ�����������ݣ���ʹ���̻߳�����Ҳ����ͬʱ���ʣ����Զ���Ϊ��̬
		// ���󼴿�
		static Terminate request;
		static TerminateResponse response;

		communicator->exchange(&request, &response, [this, doneEvent](bool) {
			::SetEvent(doneEvent);
		});

		// ����Terminate�����󱻴������ʱ���п��ܴ���ʧ�ܣ����������ȶϿ��������£����������������
		// ��������뱻ִ�У�
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
			// success Ϊ falseʱ����ζ���޷������¼�������Բ���������ͨ��
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
				// arrival action �ڽ��յ�Delivery��ʱִ��
				[this, delivery, deliveryRes](bool success) {
					// success Ϊ falseʱ����ζ���޷������¼�������Բ���������ͨ��
					if (success) {
						deliveryRes->setMsgId(delivery->messageId());
						delivery->dispatch(acceptors->smsAcceptor, acceptors->reportAcceptor);
					}
			},
				// departure action, ��deliveryRes������ɺ�ִ��
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
				// arrival action����Active���ݰ���������ʱִ��
				[](bool) {},
				// departure action, ��deliveryRes������ɺ�ִ��
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
		// ����ʱ����ʱ����������������ʱ��������һ����·����
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