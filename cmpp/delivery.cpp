#include "stdafx.h"
#include "delivery.h"
#include "serialization.h"
#include <cstdint>

using namespace comm;

namespace cmpp {
	//////////////////////////////////////////////////////////////////////////
	// Delivery

#pragma pack(1)
	struct DeliveryPod {
		uint64_t	msgId;
		uint8_t		destId[21];
		uint8_t		serviceId[10];
		uint8_t		protocolIdentifier;
		uint8_t		userdataHeadIndicator;
		uint8_t		coding;
		uint8_t		srcTerminalId[21];
		uint8_t		isStatusReport;
		uint8_t		msgLength;

		bool isReport() const { return isStatusReport==1; }
	};

	struct ReportPod {
		uint64_t	msgId;
		uint8_t		status[7];
		uint8_t		submitTime[10];
		uint8_t		doneTime[10];
		uint8_t		destTerminalId[21];
		uint8_t		smscSequence[4];
	};
#pragma pack()

	void Delivery::deserialize( StreamReader& reader ) {
		DeliveryPod delivery = {0};
		reader.read(&delivery, sizeof(delivery));
		const uint8_t IsReport = 1;

		isReport = delivery.isReport();
		msgId = delivery.msgId;
		if (isReport) {
			ReportPod report = {0};
			reader.read(&report, sizeof(report));

			static const char* status[] = {
				"DELIVRD", "EXPIRED", "DELETED", "UNDELIV", "ACCEPTD", "UNKNOWN", "REJECTD",
			};
			DeliverCode codes[] = {
				Delivered, Expired, Deleted, Undeliverable, Accepted, Unknown, Rejected,
			};
			for (int i=0; i<sizeof(status)/sizeof(status[0]); ++i) {
				if (memcmp(status[i], report.status, 7)==0) {
					statCode = codes[i];
					break;
				}
			}
		} else {
			char* msgContent = new char[delivery.msgLength+1];
			reader.read(msgContent, delivery.msgLength);
			msgContent[delivery.msgLength] = '\0';

			srcNumber = (char*)delivery.srcTerminalId;
			destNumber = (char*)delivery.destId;
			message = (char*)msgContent;

			delete msgContent;
		}
	}

	void Delivery::dispatch( function<void(const char*, const char*, const char*)>& smsAcceptor, function<void(uint64_t, DeliverCode)>& reportAcceptor ) {
		if (isReport) {
			reportAcceptor(msgId, statCode);
		} else {
			smsAcceptor(srcNumber.c_str(), destNumber.c_str(), message.c_str());
		}
	}

	uint64_t Delivery::messageId() const {
		return msgId;
	}

	//////////////////////////////////////////////////////////////////////////
	// DeliveryResponse

	uint32_t DeliveryResponse::commandId() const {
		return 0x80000005;
	}

	void DeliveryResponse::serialize( StreamWriter& writer ) const {
		const uint8_t Success = 0;

		// 和Submit的描述一致，msgId有自己的编码格式，不应该进行网络字节序的变化
		// 所以实际上应该直接写入，而不是使用整数的写入方式
		writer.write(&msgId, sizeof(msgId));
		writer<<Success;
	}

	void DeliveryResponse::setMsgId( uint64_t id ) {
		msgId = id;
	}
}



