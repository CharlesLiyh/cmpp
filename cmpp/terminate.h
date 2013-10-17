#pragma once
#include "communicator.h"

using comm::Departure;
using comm::Arrival;

namespace cmpp {
	class Terminate : public Departure {
		virtual uint32_t commandId() const;
		virtual void serialize(StreamWriter& writer) const;
	};

	class TerminateResponse : public Arrival {
		virtual void deserialize(StreamReader& reader);
	};
}