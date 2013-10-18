#pragma once
#include <functional>
#include <cstdint>
using std::function;

namespace comm {
	class SocketStream {
	public:
		SocketStream(const char* endpoint, int port);
		~SocketStream(void);

		bool open();
		void close();

		bool send(const void* payload, size_t size);
		bool recv(function<void(const uint8_t*, size_t)> callback);

	private:
		unsigned tcpSocket;
		const char* endpoint;
		int port;
	};
};

