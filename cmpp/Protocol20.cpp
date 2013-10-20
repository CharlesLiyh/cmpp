#include "stdafx.h"
#include "Protocol20.h"
#include "communicator.h"
using namespace comm;

namespace cmpp {
	Protocol20::Protocol20( const char* endpoint, int port ) {
		communicator = new Communicator(endpoint, port, 16, 3, 5);
	}

	Protocol20::~Protocol20(void) {
		delete communicator;
	}

	Communicator* Protocol20::createCommunicator() {
		return communicator;
	}

	void Protocol20::releaseObject( void* obj ) {
		// communicator��Protocol20������������������ͷ�
		if (obj!=communicator)
			delete obj;
	}
}
