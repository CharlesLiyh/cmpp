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

		// close������ͬ���ģ�ԭ�����£�
		// ���MessageGateway���ڲ��ǵ��̵߳ģ���ôcloseһ����ͬ���ģ��������
		// ���MessageGateway�Ĵ���ʹ���˶��̣߳����ǵ�һ���̲߳����ܵȴ��Լ���������Ϊ������
		// �����ѭ���������ڲ��̵߳Ľ���һ������MessageGateway��ʹ�õ��߳�֮����̣߳��������̣߳�
		// ����صġ�����ζ�ţ�close���ڵ�������˵��һ���������ġ�
		void close();

		void send(const MessageTask* task, SubmitAction response);

	private:
		Communicator* communicator;
		SocketStream* stream;
		const char* spid;
		DeliveryAcceptors* acceptors;
	};
}
