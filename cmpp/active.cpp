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
		// Active��û���κ��ֶ�
	}

	void Active::deserialize( comm::StreamReader& reader ) {
		// ͬserialize������û���κ����ݣ�����ʲô��������
	}

	//////////////////////////////////////////////////////////////////////////
	// ActiveEcho
	uint32_t ActiveEcho::commandId() const {
		return 0x80000008;
	}

	void ActiveEcho::serialize( StreamWriter& writer ) const {
		// ActiveEcho������һ����������
		const static uint8_t Nonsense = 0;
		writer<<Nonsense;
	}

	void ActiveEcho::deserialize( StreamReader& reader ) {
		// ����ActiveEcho��������������ģ����������ȡ
	}
}
