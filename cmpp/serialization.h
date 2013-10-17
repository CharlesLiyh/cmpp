#pragma once
#include <cstdint>

class StreamWriter {
public:
	StreamWriter(void* buffer);

	size_t bytesWritten() const;
	void write(const void* bytes, size_t count);
	void skip(size_t count);

	friend StreamWriter& operator<<(StreamWriter& writer, uint8_t byteVal);
	friend StreamWriter& operator<<(StreamWriter& writer, uint32_t intVal);

private:
	uint8_t* begin;
	uint8_t* current;
};

class StreamReader {
public:
	StreamReader(const void* bytes);

	void read(void* bytes, size_t count);
	void skip(size_t count);

	friend StreamReader& operator>>(StreamReader& reader, uint8_t& byteVal);
	friend StreamReader& operator>>(StreamReader& reader, uint32_t& intVal);

private:
	const uint8_t* bytes;
};