#pragma once

#include <string>
#include <map>
#include <utility>
#include <functional>
#include <cstdint>
#include <tuple>
#include <list>
using std::string;
using std::list;
using std::function;
using std::map;
using std::pair;
using std::tuple;
class AccessLock;

#ifdef CMPP_EXPORTS
#define CMPP_API __declspec(dllexport)
#else
#define CMPP_API __declspec(dllimport)
#endif

class StreamWriter;
class StreamReader;

namespace comm {
	class Departure {
	public:
		virtual ~Departure() {}
		virtual uint32_t commandId() const = 0;
		virtual void serialize(StreamWriter& writer) const = 0;
	};

	class Arrival {
	public:
		virtual ~Arrival() {}
		virtual void deserialize(StreamReader& reader) = 0;
	};

	class SocketStream;

	class CMPP_API Communicator {
	public:
		typedef function<void(bool, const string&)> ResponseAction; 
		typedef function< tuple<Arrival*, const Departure*, ResponseAction, ResponseAction>() > GivenArrivalHandler;
	public:
		Communicator(const char* endpoint, int port);
		~Communicator();

		bool open();
		void close();
		void registerPassiveArrivalHandler(uint32_t cmdId, GivenArrivalHandler handler);
		void exchange(const Departure* departure, Arrival* arrival, ResponseAction action);

	private:
		static pair<const uint8_t*, size_t> buildPayload(const Departure& departure, uint32_t sequenceId);
		void handleSending();
		void handleReceiving();

		static unsigned long __stdcall sendingThreadFunc(void* self);
		static unsigned long __stdcall receivingThreadFunc(void* self);

	private:
		SocketStream* stream;
		uint32_t sequenceId;

		map< uint32_t, GivenArrivalHandler > *givenHandlers;
		map< uint32_t, pair<Arrival*, ResponseAction> > *seqid2arrival;
		list< tuple<uint32_t, const Departure*, ResponseAction> > *departures;

		AccessLock* exchangeLock;
		void* departuresSemaphore;
		void* sendThread;
		void* recvThread;
		void* closeEvent;
	};
}