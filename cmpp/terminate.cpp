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
		// TERMINATE ����û����Ϣ�壬���Բ���Ҫ�����κ�����
	}

	//////////////////////////////////////////////////////////////////////////
	// TerminateResponse
	void TerminateResponse::deserialize(StreamReader& reader) {
		// TERMINATE_RES û����Ϣ�壬���Բ���Ҫ�����κ�����
	}
}