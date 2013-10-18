#include "stdafx.h"
#include "socketstream.h"
#include <memory>
using namespace std;

namespace comm {
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

	SocketStream::SocketStream( const char* anEndpoint, int aPort ) {
		endpoint = _strdup(anEndpoint);
		port = aPort;
	}

	SocketStream::~SocketStream() {
		delete endpoint;
	}

	bool SocketStream::open() {
		struct sockaddr_in addr;

		tcpSocket = socket( AF_INET, SOCK_STREAM, 0);

		addr.sin_family = AF_INET;
		addr.sin_port   = htons( port);
		addr.sin_addr.s_addr   = inet_addr(endpoint);

		int errCode = connect(tcpSocket, (struct sockaddr *)&addr, sizeof( addr));
		return errCode==0;
	}

	void SocketStream::close() {
		closesocket(tcpSocket);
	}

	bool SocketStream::send( const void* payload, size_t size ) {
		uint32_t sizeWrapper;
		sizeWrapper = htonl(size + sizeof(sizeWrapper));
		return process(tcpSocket, (const char*)&sizeWrapper, sizeof(sizeWrapper), &::send)
			&& process(tcpSocket, (const char*)payload, size, &::send);
	}

	bool SocketStream::recv( function<void(const uint8_t*, size_t)> callback ) {
		uint32_t size;
		if (process(tcpSocket, (char*)&size, sizeof(size), &::recv)) {
			size = ntohl(size);
			size -= sizeof(size);		// 减去数据包标注包大小的头部数据长度才是数据内容的尺寸

			auto_ptr<char> buffer(new char[size]);
			if (process(tcpSocket, buffer.get(), size, ::recv)) {
				callback((const uint8_t*)buffer.get(), size);
				return true;
			}
		}

		return false;
	}
}