#pragma once

#include <string>
#include <map>
#include <utility>
#include <functional>
#include <cstdint>
#include <tuple>
using std::string;
using std::function;
using std::map;
using std::pair;
using std::tuple;
class AccessLock;

namespace comm {
	class Stream {
	public:
		virtual ~Stream() {};
		virtual bool open(function<void(const uint8_t*, size_t)> payloadAcceptor, function<void()> errorReporter) = 0;
		virtual void close() = 0;
		virtual void send(const uint8_t* payload, size_t count, function<void(bool)> doneAction) = 0;
	};

	class Departure;
	class StreamWriter {
	public:
		StreamWriter(void* buffer);

		size_t bytesWritten() const;
		void write(const void* bytes, size_t count);
		void skip(size_t count);

		friend StreamWriter& operator<<(StreamWriter& writer, uint8_t byteVal);
		friend StreamWriter& operator<<(StreamWriter& writer, uint32_t intVal);
		friend StreamWriter& operator<<(StreamWriter& writer, uint64_t bigIntVal);
		friend StreamWriter& operator<<(StreamWriter& writer, const Departure& request);

	private:
		uint8_t* begin;
		uint8_t* current;
	};

	class Arrival;
	class StreamReader {
	public:
		StreamReader(const void* bytes);

		void read(void* bytes, size_t count);
		void skip(size_t count);

		friend StreamReader& operator>>(StreamReader& reader, uint8_t& byteVal);
		friend StreamReader& operator>>(StreamReader& reader, uint32_t& intVal);
		friend StreamReader& operator>>(StreamReader& reader, Arrival& arrival);

	private:
		const uint8_t* bytes;
	};

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

	class Communicator {
	public:
		typedef function<void(bool, const string&)> ResponseAction; 
		typedef function< tuple<Arrival*, const Departure*, ResponseAction, ResponseAction>() > GivenArrivalHandler;
	public:
		Communicator(Stream& stream);
		~Communicator();

		bool open();
		void close();
		void registerPassiveArrivalHandler(uint32_t cmdId, GivenArrivalHandler handler);
		void exchange(const Departure* departure, Arrival* arrival, ResponseAction action);

	private:
		void send(const Departure& departure, uint32_t departureId, ResponseAction action);
		static pair<uint8_t*, int> buildPayload(const Departure& departure, uint32_t sequenceId);

	private:
		Stream& stream;
		map< uint32_t, pair<Arrival*, ResponseAction> > seqid2arrival;
		map<uint32_t, GivenArrivalHandler> givenHandlers;
		uint32_t givenId;
		GivenArrivalHandler givenHandler;
		uint32_t sequenceId;
		AccessLock* exchangeLock;
	};
}