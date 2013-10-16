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
	class SocketStream;
	typedef function<void(bool success, const char* failure)> CommonAction;
	typedef function<void(bool success, const char* failure, uint64_t)> SubmitAction;
	typedef function<void(const char*, const char*, const char*)> SMSAcceptor;
	typedef function<void(uint64_t, DeliverCode)> ReportAcceptor;
	struct DeliveryAcceptors;

	class CMPP_API MessageGateway {
	public:
		MessageGateway(const char* endpoint, int port, SMSAcceptor smsAcceptor, ReportAcceptor reportAcceptor);
		~MessageGateway();

		void open(const SPID *spid, CommonAction response);

		// close必须是同步的，原因如下：
		// 如果MessageGateway的内部是单线程的，那么close一定是同步的，无须解释
		// 如果MessageGateway的处理使用了多线程，考虑到一个线程不可能等待自己结束，因为这样会
		// 造成死循环。所以内部线程的结束一定是由MessageGateway所使用的线程之外的线程（例如主线程）
		// 来监控的。这意味着，close对于调用者来说，一定是阻塞的。
		void close();

		void send(const MessageTask* task, SubmitAction response);

	private:
		Communicator* communicator;
		SocketStream* stream;
		const char* spid;
		DeliveryAcceptors* acceptors;
	};
}
