#include "stdafx.h"
#include <ctime>
#include "cmpp.h"
#include "connect.h"
#include "md5.h"
#include "communicator.h"
#include "serialization.h"
using namespace comm;

namespace cmpp {
	//////////////////////////////////////////////////////////////////////////
	// Connect
	Connect::Connect(const SPID* anId, time_t tmStamp) {
		spid = anId;
		timeStamp = tmStamp;
	}

	void Connect::serialize(StreamWriter& writer ) const
	{
		// {
		// 		unsigned char spid[6];						//	9开头的6位企业码
		// 		unsigned char digest[16];					//	MD5数字签名
		// 		unsigned char ver;							//	版本
		// 		int timestamp;
		// 	}
		writer.write(spid->username(), 6);

		MD5 ctx;
		char authsrc[50], *pos, timestr[20];

		//	计算单向HASH函数的值
		memset( authsrc, 0, sizeof(authsrc));
		memcpy_s( authsrc, sizeof(authsrc), spid->username(), 6);
		pos = authsrc + strlen(spid->username()) + 9;
		memcpy_s( pos, sizeof(authsrc)-(pos-authsrc), spid->password(), strlen(spid->password()));
		pos += strlen(spid->password());

		tm now;
		localtime_s(&now, &timeStamp);
		sprintf_s( timestr, "%02d%02d%02d%02d%02d",
			now.tm_mon + 1,
			now.tm_mday,
			now.tm_hour,
			now.tm_min,
			now.tm_sec);

		memcpy_s(pos, sizeof(authsrc)-(pos-authsrc), timestr, strlen(timestr));
		pos += strlen( timestr);

		ctx.update( (uint8_t*)authsrc, pos - authsrc );
		ctx.finalize();

		uint8_t digest[16];
		ctx.raw_digest(digest);
		writer.write(digest, 16);

		writer<<(uint8_t)0x30<<(uint32_t)atol(timestr);
	}

	//////////////////////////////////////////////////////////////////////////
	// LoginResponse

	ConnectResponse::ConnectResponse() {
		version = -1;
		verified = false;
	}

	void ConnectResponse::deserialize(StreamReader& reader ) {
		uint8_t status, ver;
		uint8_t certificationData[17];
		reader>>status;
		reader.read(certificationData, sizeof(certificationData));
		certificationData[16] = '\0';
		reader>>ver;

		const char* messages[] = {
			"消息结构错误",
			"非法源地址",
			"认证错误",
			"版本太高",
			"其它错误",
		};

		const int Success = 0;
		verified = status==Success;
		if (!verified) {
			message = messages[status-1];
			ismgCertification = (const char*)certificationData;
			version = ver;
		}
	}
}