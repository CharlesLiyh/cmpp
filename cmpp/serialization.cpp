#include "stdafx.h"
#include "serialization.h"

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
