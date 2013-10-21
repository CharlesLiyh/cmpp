#pragma once
#include <functional>
#include "communicator.h"
#include "conceptions.h"
using std::function;

class StreamWriter;

namespace cmpp {
	class Query : public comm::Departure {
	public:
		Query(const string& tillDate);
		void queryForAmounts();
		void queryForService(const string& service);

		virtual uint32_t commandId() const;

		virtual void serialize( StreamWriter& writer ) const;

	private:
		string dateString;
		function<void(StreamWriter&)> specificWriter;
	};

	class QueryResponse : public comm::Arrival {
	public:
		QueryResponse(Statistics* statistics);

		virtual void deserialize( StreamReader& reader );

	private:
		Statistics* statistics;
	};
}

