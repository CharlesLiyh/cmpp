#include "stdafx.h"
#include <memory>
#include <winsock2.h>
#include "SocketStream.h"
#include "AccessLock.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

namespace cmpp {
	//////////////////////////////////////////////////////////////////////////
	// SocketStream

	SocketStream::SocketStream( const string& anEndpoint, int aPort ) {
		endpoint = anEndpoint;
		port = aPort;
		closeEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		exportsLock = new AccessLock();
	}

	SocketStream::~SocketStream() {
		::CloseHandle(closeEvent);
		delete exportsLock;
	}

	void SocketStream::handleExports() {
		bool moreJobs = true;
		while (moreJobs) {
			HANDLE events[] = { closeEvent, exportsSemaphore };
			DWORD eventIdx = ::WaitForMultipleObjects(sizeof(events)/sizeof(events[0]), events, FALSE, INFINITE);
			eventIdx -= WAIT_OBJECT_0;
			if (eventIdx==0) {
				moreJobs = false;
			} else {
				exportsLock->lock([this](){
					auto payloadTuple = exports.front();
					const uint8_t* payload = get<0>(payloadTuple);
					size_t count = get<1>(payloadTuple);
					function<void(bool)>& doneAction = get<2>(payloadTuple);

					// 整个数据包的长度是业务数据的长度+包头长度 = count + sizeof(sizeWrap)
					int32_t sizeWrap = htonl(count + sizeof(sizeWrap));
					doneAction(sendWhole((uint8_t*)&sizeWrap, sizeof(sizeWrap)) && sendWhole(payload, count));
					exports.pop_front();
				});
			}
		}
	}

	void SocketStream::handleIncomes() {
		bool success = true;
		while(success) {
			int32_t sizeWrap;
			success = success && receiveWhole((uint8_t*)&sizeWrap, sizeof(sizeWrap));
			if (!success)
				break;

			size_t payloadSize = ntohl(sizeWrap);
			payloadSize -= sizeof(sizeWrap);

			auto_ptr<uint8_t> buff(new uint8_t[payloadSize]);

			success = success && receiveWhole(buff.get(), payloadSize);
			if (!success)
				break;

			printf("income package, size:%d\n", payloadSize);
			for (size_t i=0; i<payloadSize; ++i) {
				printf("%02X ", buff.get()[i]);
				if ( ((i+1)%16)==0 ) {
					printf("\n");
				}
			}
			printf( payloadSize%16==0 ? "\n" : "\n\n");

			payloadAccesptor(buff.get(), payloadSize);
		}
	}

	DWORD SocketStream::sendFunc(LPVOID self) {
		((SocketStream*)self)->handleExports();
		return 0;
	}

	DWORD SocketStream::recvFunc(LPVOID self) {
		((SocketStream*)self)->handleIncomes();
		return 0;
	}

	bool SocketStream::open( function<void(const uint8_t*, size_t ) > anAcceptor, function<void()> aReporter ) {
		payloadAccesptor = anAcceptor;
		errorReporter = aReporter;

		setupSocket();

		exportsSemaphore = ::CreateSemaphore(NULL, 0, 65535, NULL);
		::ResetEvent(closeEvent);

		DWORD threadId;
		sendThread = ::CreateThread(NULL, 0, sendFunc, this, 0, &threadId);
		recvThread = ::CreateThread(NULL, 0, recvFunc, this, 0, &threadId);

		return true;
	}

	void SocketStream::close() {
		::SetEvent(closeEvent);
		closesocket(tcpSocket);

		// 等待数据发送线程‘寿终正寝’
		::WaitForSingleObject(sendThread, INFINITE);
		::WaitForSingleObject(recvThread, INFINITE);

		::CloseHandle(exportsSemaphore);
	}

	void SocketStream::send( const uint8_t* payload, size_t count, function<void(bool)> doneAction ) {
		exportsLock->lock([this, payload, count, &doneAction]() {
			exports.push_back(make_tuple(payload, count, doneAction));
		});

		LONG preCount;
		::ReleaseSemaphore(exportsSemaphore, 1, &preCount);
	}

	bool SocketStream::setupSocket() {
		struct sockaddr_in addr;

		tcpSocket = socket( AF_INET, SOCK_STREAM, 0);

		addr.sin_family = AF_INET;
		addr.sin_port   = htons( port);
		addr.sin_addr.s_addr   = inet_addr(endpoint.c_str());

		int errCode = connect(tcpSocket, (struct sockaddr *)&addr, sizeof( addr));
		return errCode!=0;
	}

	template<class PayloadType>
	bool process(SOCKET tcpSocket, PayloadType payload, size_t total, int (__stdcall *handler)(SOCKET, PayloadType, int, int)) {
		size_t remain = total;
		bool success = true;
		while(success && remain>0) {
			int handled = handler(tcpSocket, payload + total - remain, remain, 0);
			if (handled>0) {
				remain -= handled;
			} else {
				remain = 0;
				success = false;
			}
		}

		return success;
	}

	bool SocketStream::sendWhole( const uint8_t* payload, size_t total ) {
		return process(tcpSocket, (const char*)payload, total, &::send);
	}

	bool SocketStream::receiveWhole( uint8_t* buff, size_t total ) {
		return process(tcpSocket, (char*)buff, total, &::recv);
	}
}
