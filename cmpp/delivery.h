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

	// 虽然DeliveryResponse是用于向ISMG作echo用途的数据包，这也是把它命名为Response的原因。
	// 但，从SP的角度出发，它实际上是一个待发送的数据包。所以它必须是一个Request
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