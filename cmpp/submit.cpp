#include "stdafx.h"
#include <cstdint>
#include "submit.h"
#include "conceptions.h"
#include "serialization.h"
using namespace comm;

namespace cmpp {
#pragma pack(1)
	struct SMSPod {
		uint64_t msgid;						//	信息标识
		uint8_t pkgtotal;					//	相同Msg_Id的信息总条数
		uint8_t pkgnumber;					//	相同Msg_Id的信息序号
		uint8_t delivery;					//	是否要求返回状态确认报告
		uint8_t msglevel;					//	信息级别
		uint8_t serviceid[10];				//	业务类型
		uint8_t feeusertype;				//	计费用户类型字段
		uint8_t feenumber[21];				//	被计费用户的号码
		uint8_t tppid;						//	TP-Protocol-Identifier
		uint8_t tpudhi;						//	TP-User-Data-Header-Indicator
		uint8_t msgfmt;						//	信息格式，8：UCS2编码
		uint8_t msgsrc[6];					//	信息内容来源(SP_Id)
		uint8_t feetype[2];					//	资费类别
		uint8_t feecode[6];					//	资费代码（以分为单位）
		uint8_t validtime[17];				//	存活有效期，参见SMPP3.3
		uint8_t attime[17];					//	定时发送时间，参见SMPP3.3 
		char srcnumber[21];					//	源号码
		uint8_t desttotal;					//	接收信息的用户数量(===1)
		char destnumbers[21];				//	接收短信的MSISDN号码
		uint8_t msglen;						//	信息长度(<=140)
		
		// 文档中定义了下面两个字段， 但考虑到这个包的长度是可变的，所以在SubmitSMS中进行了特殊处理
		// char msgcontent[160];			//	信息内容
		// char reserve[20];				//	保留
	};
#pragma pack()

	SubmitSMS::SubmitSMS(const MessageTask* aTask, const string& theSpid) : spid(theSpid) {
		task = aTask;
	}

	uint32_t SubmitSMS::commandId() const {
		return 0x00000004;
	}

	void SubmitSMS::serialize( StreamWriter& writer ) const {
		const uint8_t WantDelivery = 1;
		const uint64_t AutoGenerateByServer = 0;
		const char* FixedLogo = "lanzsoft";
		
		// 该语法将对整个sms对象置0数据，仅对struct有效
		SMSPod sms = {0};	
		sms.msgid		= AutoGenerateByServer;	// 信息标识，由SP侧短信网关本身产生，本处填空。
		sms.delivery	= WantDelivery;
		sms.pkgtotal	= 1;
		sms.pkgnumber	= 1;
		sms.desttotal	= 1;
		memcpy_s(sms.destnumbers, sizeof(sms.destnumbers), task->destNumbers(), strlen(task->destNumbers()));
		memcpy_s(sms.msgsrc, sizeof(sms.msgsrc), spid.c_str(), spid.length());
		sms.msglen = task->textLength();

		writer.write(&sms, sizeof(sms));
		writer.write(task->messageText(), task->textLength());
		writer.write(FixedLogo, strlen(FixedLogo));
	}

	//////////////////////////////////////////////////////////////////////////
	// SubmitSMSResponse

	void SubmitSMSResponse::deserialize( StreamReader& reader ) {
		// id的读取不能使用一般整数的读取方式，因为id实际上是一个经过编码、压缩过的数字
		// 它并不是一个真正意义上的整数。所以在读取时，也必须保留它的编码顺序，而从reader
		// 读取整数会导致从网络字节序到主机字节序的转换，这会破坏id的编码方式。
		reader.read(&id, sizeof(id));

		uint8_t resCode;
		reader>>resCode;

		static const char* failures[] = {
			"Bad structural", 
			"Bad command id",
			"Duplicated message id",
			"Bad message length",
			"Bad expense code",
			"Exceed max message length",
			"Bad Service code",
			"QoS control failed",
			"Unknown failure",
		};

		success = resCode==0;
		if (!success)
			failure = failures[min(resCode-1, 8)];
	}

	bool SubmitSMSResponse::isSuccess() const{
		return success;
	}

	uint64_t SubmitSMSResponse::messageId() const{
		return id;
	}

	const string& SubmitSMSResponse::reason() const{
		return failure;
	}
}