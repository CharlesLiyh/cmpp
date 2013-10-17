#pragma once
#include "communicator.h"
#include "conceptions.h"

namespace cmpp {
	class Delivery : public comm::Arrival {
	public:
		static const uint32_t CommandId = 0x00000005;

		virtual void deserialize( StreamReader& reader );

		uint64_t messageId() const;

		void dispatch(function<void(const char*, const char*, const char*)>& smsAcceptor,
			function<void(uint64_t, DeliverCode)>& reportAcceptor);
	private:
		bool isReport;
		uint64_t msgId;
		string srcNumber;
		string destNumber;
		string message;
		DeliverCode statCode;
	};

	// ��ȻDeliveryResponse��������ISMG��echo��;�����ݰ�����Ҳ�ǰ�������ΪResponse��ԭ��
	// ������SP�ĽǶȳ�������ʵ������һ�������͵����ݰ���������������һ��Request
	class DeliveryResponse : public comm::Departure {
	public:
		void setMsgId(uint64_t id);
	protected:
		virtual uint32_t commandId() const;

		virtual void serialize( StreamWriter& writer ) const;

	private:
		uint64_t msgId;
	};
}