#pragma once

#include <string>
#include <map>
#include <utility>
#include <functional>
#include <cstdint>
#include <tuple>
#include <list>
#include "SocketStream.h"
#include "AccessLock.h"
using std::string;
using std::list;
using std::function;
using std::map;
using std::pair;
using std::tuple;
class AccessLock;
class StreamWriter;
class StreamReader;

// #define DEBUG_COMM

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

	class Communicator {
	public:
		typedef function<void(bool, const string&)> ResponseAction; 
		typedef function< tuple<Arrival*, const Departure*, ResponseAction, ResponseAction>() > GivenArrivalHandler;
		typedef function<void(const string&)> ErrorReporter;

	public:
		Communicator(const char* endpoint, int port, size_t sendingWndSize=16, int maxLives = 3, double timeout = 60.0);
		~Communicator();

		bool open(ErrorReporter reporter);
		void close();

		void stopStuffs();

		void registerPassiveArrivalHandler(uint32_t cmdId, GivenArrivalHandler handler);
		void exchange(const Departure* departure, Arrival* arrival, ResponseAction action);

	private:
		bool send(tuple<uint32_t, const Departure*, ResponseAction>& departTuple);
		bool sendActiveDeparture();
		bool sendPassiveDeparture();
		void activeDepartureTimer(HANDLE timer);
		void loopSending();
		void loopReceiving();
		void loopCombineShootingEvent();
		void reportError(const char* message);
		void handleArrivalRequest( uint32_t commandId, const uint8_t* payload, uint32_t arrivalId );
		void handleArrivalResponse( uint32_t arrivalId, const uint8_t* payload );

		static pair<const uint8_t*, size_t> buildPayload(const Departure& departure, uint32_t sequenceId);
		static unsigned long __stdcall sendingThreadFunc(void* self);
		static unsigned long __stdcall receivingThreadFunc(void* self);
		static unsigned long __stdcall waitForShootingThread(void* self);
		static void __stdcall interruptWaiting(unsigned long self);

		void dump(const char* msg);
#ifdef DEBUG_COMM
		//////////////////////////////////////////////////////////////////////////
		// debug
		static unsigned long __stdcall dumpThread(void* self);
		void dumpLoop();
#endif
	private:
		bool stopAllStuffs;
		bool errorAlreadyReported;
		SocketStream stream;
		uint32_t sequenceId;
		double timeout;
		ErrorReporter errorReporter;

		map< uint32_t, GivenArrivalHandler >givenHandlers;
		map< uint32_t, pair<Arrival*, ResponseAction> >seqid2arrival;
		list< tuple<uint32_t, const Departure*, ResponseAction> >activeDepartures;
		list< tuple<uint32_t, const Departure*, ResponseAction> >passiveDepartures;

		AccessLock departuresLock;
		HANDLE activeDepartSem;
		AccessLock passiveDepartLock;
		HANDLE passiveDepartSem;
		HANDLE timersSem;
		HANDLE shootingSem;
		HANDLE sendThread;
		HANDLE recvThread;
		HANDLE shootWaitingThread;

		HANDLE* retryTimers;
		size_t timersCount;
		int maxLives;
		map<uint32_t, pair<HANDLE, int> > seqid2retryStuffs;
		map< HANDLE, tuple<uint32_t, const Departure*, ResponseAction> > timer2departure;
		list<HANDLE> freeTimers;
	};
}