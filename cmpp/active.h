#pragma once
#include "communicator.h"

namespace cmpp {
	class Active : public comm::Departure, public comm::Arrival {
	public:
		static const uint32_t CommandId = 0x00000008;

	protected:
		virtual uint32_t commandId() const;
		virtual void serialize( comm::StreamWriter& writer ) const;
		virtual void deserialize( comm::StreamReader& reader );
	};

	class ActiveEcho : public comm::Departure, public comm::Arrival {
		virtual uint32_t commandId() const;
		virtual void serialize( comm::StreamWriter& writer ) const;
		virtual void deserialize( comm::StreamReader& reader );
	};
}