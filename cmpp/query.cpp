#include "stdafx.h"
#include "query.h"
#include "serialization.h"

namespace cmpp {
	Query::Query( const string& tillDate ) {
		dateString = tillDate;
		specificWriter = [](StreamWriter&) { throw "��������Ϊ��ѯҵ������(queryForAmount)�������ҵ������ѯ(queryForService)"; };
	}

	void Query::queryForAmounts() {
		specificWriter = [](StreamWriter& writer) {
			writer<<(uint8_t)0x00;
			writer.skip(10);
		};
	}

	void Query::queryForService( const string& service ) {
		specificWriter = [service](StreamWriter& writer) {
			writer<<(uint8_t)0x01;
			writer.write(service.c_str(), service.size());

			// Service �����ܹ�ռ10���ֽڵĿռ䣬����ṩ��service����10�ֽڣ�����벹��
			writer.skip(10 - service.size());
		};
	}

	uint32_t Query::commandId() const {
		return 0x00000006;
	}

	void Query::serialize( StreamWriter& writer ) const {
		writer.write(dateString.c_str(), dateString.size());
		specificWriter(writer);

		const int ReservedCount = 8;
		writer.skip(ReservedCount);
	}


	QueryResponse::QueryResponse( Statistics* stat ) {
		statistics = stat;
	}

	void QueryResponse::deserialize( StreamReader& reader ) {
		char dateBuff[9] = {0};
		uint8_t forAmount;
		char serviceCode[11] = {0};
		uint32_t msgAmount, userAmount, sr, qr, fr, ss, qs, fs;
		reader.read(dateBuff, 8);
		reader>>forAmount;
		reader.read(serviceCode, 10);
		reader>>msgAmount>>userAmount>>sr>>qr>>fr>>ss>>qs>>fs;

		statistics->fillsWith(dateBuff, serviceCode, msgAmount, userAmount, sr, qr, fr, ss, qs, fs);
	}
}
