#include "stdafx.h"
#include "active.h"
using namespace comm;

namespace cmpp {
	//////////////////////////////////////////////////////////////////////////
	// Active
	uint32_t Active::commandId() const {
		return CommandId;
	}

	void Active::serialize( comm::StreamWriter& writer ) const {
		// Active包没有任何字段
	}

	void Active::deserialize( comm::StreamReader& reader ) {
		// 同serialize，由于没有任何数据，所以什么都不用做
	}

	//////////////////////////////////////////////////////////////////////////
	// ActiveEcho
	uint32_t ActiveEcho::commandId() const {
		return 0x80000008;
	}

	void ActiveEcho::serialize( StreamWriter& writer ) const {
		// ActiveEcho仅包含一个保留数据
		const static uint8_t Nonsense = 0;
		writer<<Nonsense;
	}

	void ActiveEcho::deserialize( StreamReader& reader ) {
		// 由于ActiveEcho的数据是无意义的，所以无需读取
	}
}
