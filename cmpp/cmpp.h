#pragma once

#include "conceptions.h"
#include <functional>
#include <cstdint>

#ifdef CMPP_EXPORTS
#define CMPP_API __declspec(dllexport)
#else
#define CMPP_API __declspec(dllimport)
#endif

namespace comm {
	class Communicator;
}

using std::function;
using comm::Communicator;

namespace cmpp {

	class SPID;
	class MessageTask;
	class Query;
	class QueryResponse;
	class Protocol20;
	struct DeliveryAcceptors;

	typedef function<void(bool success, const char* failure)> CommonAction;
	typedef function<void(const char*)> ErrorReporter;
	typedef function<void(bool success, const char* failure, uint64_t)> SubmitAction;
	typedef function<void(const char*, const char*, const char*)> SMSAcceptor;
	typedef function<void(uint64_t, DeliverCode)> ReportAcceptor;
	typedef function<void(bool)> SimpleAction;
	typedef function<void(Statistics& stat)> QueryAction;

	class CMPP_API MessageGateway {
	public:
		MessageGateway(Protocol20* protocol, SMSAcceptor smsAcceptor, ReportAcceptor reportAcceptor, ErrorReporter errorReporter, float heartbeatInterval = 180.0);
		~MessageGateway();

		bool open(const SPID *spid, CommonAction action);
		void close();
		void send(const MessageTask* task, SubmitAction action);
		void cancel(uint64_t taskId, SimpleAction action);
		void queryForAmounts(uint32_t year, uint8_t month, uint8_t day, QueryAction action);
		void queryForService(uint32_t year, uint8_t month, uint8_t day, const char* serviceCode, QueryAction action);

	private:
		// ∑¿÷πøΩ±¥MessageGateway∂‘œÛ
		MessageGateway(const MessageGateway&);
		static unsigned long __stdcall heartbeatThread(void* self);
		void keepActive();
		void stopHeartbeat();
		void authenticate( const SPID* account, CommonAction& action );
		void registerDeliveriesHandler();
		void registerActiveHandler();
		void exchangeQuery(Query* request, QueryResponse* res, Statistics* stat, QueryAction act);
	private:
		Protocol20* protocol;
		Communicator* communicator;
		const char* spid;
		DeliveryAcceptors* acceptors;
		void* heartbeatThreadHandle;
		void* heartbeatStopEvent;
		float heartbeatInterval;
	};
}
