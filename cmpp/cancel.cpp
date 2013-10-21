#include "stdafx.h"
#include "cancel.h"
#include "serialization.h"
using namespace comm;

namespace cmpp {
	//////////////////////////////////////////////////////////////////////////
	// Cancel
	Cancel::Cancel(uint64_t id) {
		msgId = id;
	}

	uint32_t Cancel::commandId() const {
		return 0x00000007;
	}

	void Cancel::serialize( StreamWriter& writer ) const {
		writer.write(&msgId, sizeof(msgId));
	}

	//////////////////////////////////////////////////////////////////////////
	// CancelResponse

	void CancelResponse::deserialize( StreamReader& reader ) {
		uint8_t succeedTag;
		reader>>succeedTag;

		succeed = succeedTag==0;
	}

	bool CancelResponse::isSucceed() const {
		return succeed;
	}
}

