#pragma once

#ifdef CMPP_EXPORTS
#define CMPP_API __declspec(dllexport)
#else
#define CMPP_API __declspec(dllimport)
#endif

namespace comm {
	class Communicator;
}
using comm::Communicator;

namespace cmpp {
	class CMPP_API Protocol20 {
	public:
		Protocol20(const char* endpoint, int port);
		~Protocol20(void);

		Communicator* createCommunicator();
		void releaseObject(void* obj);
	private:
		Communicator* communicator;
	};
}
