#pragma once
#include <cstdint>
#include "communicator.h"

namespace cmpp {
	class Cancel : public comm::Departure {
	public:
		Cancel(uint64_t msgId);

		virtual uint32_t commandId() const;

		virtual void serialize( StreamWriter& writer ) const;

	private:
		uint64_t msgId;
	};

	class CancelResponse : public comm::Arrival {
	public:
		bool isSucceed() const;

		virtual void deserialize( StreamReader& reader );

	private:
		bool succeed;
	};
}

