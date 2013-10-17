#include "stdafx.h"
#include <cstdint>
#include "submit.h"
#include "conceptions.h"
#include "serialization.h"
using namespace comm;

namespace cmpp {
#pragma pack(1)
	struct SMSPod {
		uint64_t msgid;						//	��Ϣ��ʶ
		uint8_t pkgtotal;					//	��ͬMsg_Id����Ϣ������
		uint8_t pkgnumber;					//	��ͬMsg_Id����Ϣ���
		uint8_t delivery;					//	�Ƿ�Ҫ�󷵻�״̬ȷ�ϱ���
		uint8_t msglevel;					//	��Ϣ����
		uint8_t serviceid[10];				//	ҵ������
		uint8_t feeusertype;				//	�Ʒ��û������ֶ�
		uint8_t feenumber[21];				//	���Ʒ��û��ĺ���
		uint8_t tppid;						//	TP-Protocol-Identifier
		uint8_t tpudhi;						//	TP-User-Data-Header-Indicator
		uint8_t msgfmt;						//	��Ϣ��ʽ��8��UCS2����
		uint8_t msgsrc[6];					//	��Ϣ������Դ(SP_Id)
		uint8_t feetype[2];					//	�ʷ����
		uint8_t feecode[6];					//	�ʷѴ��루�Է�Ϊ��λ��
		uint8_t validtime[17];				//	�����Ч�ڣ��μ�SMPP3.3
		uint8_t attime[17];					//	��ʱ����ʱ�䣬�μ�SMPP3.3 
		char srcnumber[21];					//	Դ����
		uint8_t desttotal;					//	������Ϣ���û�����(===1)
		char destnumbers[21];				//	���ն��ŵ�MSISDN����
		uint8_t msglen;						//	��Ϣ����(<=140)
		
		// �ĵ��ж��������������ֶΣ� �����ǵ�������ĳ����ǿɱ�ģ�������SubmitSMS�н��������⴦��
		// char msgcontent[160];			//	��Ϣ����
		// char reserve[20];				//	����
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
		
		// ���﷨��������sms������0���ݣ�����struct��Ч
		SMSPod sms = {0};	
		sms.msgid		= AutoGenerateByServer;	// ��Ϣ��ʶ����SP��������ر��������������ա�
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
		// id�Ķ�ȡ����ʹ��һ�������Ķ�ȡ��ʽ����Ϊidʵ������һ���������롢ѹ����������
		// ��������һ�����������ϵ������������ڶ�ȡʱ��Ҳ���뱣�����ı���˳�򣬶���reader
		// ��ȡ�����ᵼ�´������ֽ��������ֽ����ת��������ƻ�id�ı��뷽ʽ��
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