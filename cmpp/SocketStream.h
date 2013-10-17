#pragma once

#include <functional>
#include <string>
#include <list>
#include <cstdint>
#include "communicator.h"
#include <tuple>

using std::function;
using std::string;
using std::list;
using std::tuple;
class AccessLock;

namespace cmpp {
	class SocketStream : public comm::Stream {
	public:
		SocketStream(const string& endpoint, int port);
		~SocketStream();

		virtual bool open(function<void(const uint8_t*, size_t ) > anAcceptor, function<void()> aReporter);

		virtual void close();

		virtual void send( const uint8_t* payload, size_t count, function<void(bool)> doneAction);

	private:
		void handleSending();
		void handleReceiving();
		bool setupSocket();
		bool sendTrunk(const uint8_t* payload, size_t total);
		bool receiveTrunk(uint8_t* buff, size_t total);

		static DWORD __stdcall sendingThreadFunc(LPVOID self);
		static DWORD __stdcall receivingThreadFunc(LPVOID self);
	private:
		unsigned int tcpSocket;
		string endpoint;
		int port;
		function<void(const uint8_t*, size_t)> payloadAccesptor;
		function<void()> errorReporter;

		list< tuple<const uint8_t*, size_t, function<void(bool)> > > exports;
		HANDLE exportsSemaphore;
		HANDLE sendThread;
		AccessLock* exportsLock;

		HANDLE recvThread;
		HANDLE closeEvent;
	};
}