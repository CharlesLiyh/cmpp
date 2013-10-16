#include "stdafx.h"
#include "terminate.h"
#include "cmpp.h"
#include "communicator.h"

using namespace comm;

namespace cmpp {
	//////////////////////////////////////////////////////////////////////////
	// TerminateRequest
	uint32_t Terminate::commandId() const {
		return 0x00000002;
	}

	void Terminate::serialize( StreamWriter& writer ) const {
		// TERMINATE 请求没有消息体，所以不需要编码任何数据
	}

	//////////////////////////////////////////////////////////////////////////
	// TerminateResponse
	void TerminateResponse::deserialize(StreamReader& reader) {
		// TERMINATE_RES 没有消息体，所以不需要解析任何数据
	}
}