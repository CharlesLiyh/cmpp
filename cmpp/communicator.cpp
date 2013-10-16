#include "stdafx.h"
#include "communicator.h"
#include <utility>
#include "AccessLock.h"
#include <winsock2.h>
using namespace std;

namespace comm {
	StreamWriter::StreamWriter(void* buffer) {
		begin = current = (uint8_t*)buffer;
	}

	size_t StreamWriter::bytesWritten() const {
		return current - begin;
	}

	void StreamWriter::write(const void* bytes, size_t count) {
		memcpy_s(current, count, bytes,count);
		current += count;
	}

	void StreamWriter::skip(size_t count) {
		current += count;
	}

	StreamWriter& operator<<(StreamWriter& stream, uint8_t byteVal) {
		*stream.current = byteVal;
		stream.current += sizeof(byteVal);
		return stream;
	}

	StreamWriter& operator<<(StreamWriter& stream, uint32_t intVal) {
		*((int32_t*)stream.current) = htonl(intVal);
		stream.current += sizeof(intVal);
		return stream;
	}

	StreamWriter& operator<<(StreamWriter& writer, uint64_t bigIntVal) {
		*((uint64_t*)writer.current) = htonll(bigIntVal);
		writer.current += sizeof(bigIntVal);
		return writer;
	}

	StreamWriter& operator<<(StreamWriter& stream, const Departure& request) {
		request.serialize(stream);
		return stream;
	}

	//////////////////////////////////////////////////////////////////////////
	// StreamReader
	StreamReader::StreamReader(const void* someBytes) {
		bytes = (uint8_t*)someBytes;
	}

	void StreamReader::read(void* buffer, size_t count) {
		memcpy_s(buffer, count, bytes, count);
		bytes += count;
	}

	void StreamReader::skip( size_t count ) {
		bytes += count;
	}

	StreamReader& operator>>(StreamReader& reader, uint8_t& byteVal) {
		byteVal = *(reader.bytes++);
		return reader;
	}

	StreamReader& operator>>(StreamReader& reader, uint32_t& intVal) {
		int32_t netInt = *((int*)reader.bytes);
		intVal = ntohl(netInt);

		reader.bytes += 4;
		return reader;
	}

	StreamReader& operator>>(StreamReader& reader, Arrival& response) {
		response.deserialize(reader);
		return reader;
	}

	//////////////////////////////////////////////////////////////////////////
	// Communicator

	Communicator::Communicator(Stream& aStream) : stream(aStream) {
		exchangeLock = new AccessLock();
	}

	Communicator::~Communicator() {
		delete exchangeLock;
	}

	bool Communicator::open() {
		sequenceId = 0;

		return stream.open(
			// payload acceptor
			[this](const uint8_t* payload, size_t count){
				StreamReader reader(payload);

				uint32_t commandId, arrivalId;
				reader>>commandId>>arrivalId;

				if (commandId==givenId) {
					auto handlerPair = givenHandler();
					Arrival* specialResponse = get<0>(handlerPair);
					const Departure* departure = get<1>(handlerPair);
					ResponseAction& arrivalAction = get<2>(handlerPair);
					ResponseAction& departureAction = get<3>(handlerPair);

					reader>>*specialResponse;
					arrivalAction(true, "");
					send(*departure, arrivalId, departureAction);
					
				} else {
					auto responsePair = seqid2arrival.at(arrivalId);
					Arrival* response = responsePair.first;
					ResponseAction action = responsePair.second;
					reader>>*response;
					action(true, "");

					seqid2arrival.erase(arrivalId);
				}
		},
			// error reporter
			[](){
				// TODO: 处理数据接收异常
		});
	}

	void Communicator::close() {
		stream.close();
	}

	pair<uint8_t*, int> Communicator::buildPayload(const Departure& request, uint32_t seqId) {
		uint8_t buffer[1024];
		memset(buffer, 0, sizeof(buffer));

		StreamWriter writer(buffer);
		writer<<request.commandId()<<seqId<<request;

		int payloadSize = writer.bytesWritten();

		uint8_t* payload = new uint8_t[payloadSize];
		memcpy_s(payload, payloadSize, buffer, payloadSize);

		return make_pair(payload, payloadSize);
	}

	void Communicator::exchange(const Departure* request, Arrival* response, ResponseAction action) {
		uint32_t departureId;
		exchangeLock->lock([this, &departureId, response, &action](){
			departureId = sequenceId++;
			seqid2arrival.insert(make_pair(departureId, make_pair(response, action)));
		});

		send(*request, departureId, [](bool, const string&){});
	}

	void Communicator::registerPassiveArrivalHandler(uint32_t cmdId, GivenArrivalHandler handler) {
		givenId = cmdId;
		givenHandler = handler;
	}

	void Communicator::send( const Departure& departure, uint32_t departureId, ResponseAction action ) {
		pair<uint8_t*, int> payloadPair = buildPayload(departure, departureId);
		uint8_t* payload = payloadPair.first;
		int payloadSize = payloadPair.second;

		stream.send(payload, payloadSize, [payload, action](bool){
			delete[] payload; 
			action(true, "");
		});
	}

}