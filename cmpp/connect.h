#pragma once
#include <string>
#include "cmpp.h"
#include "communicator.h"
#include "conceptions.h"

namespace cmpp {
	class SPID;

	class Connect : public comm::Departure {
	public:
		Connect(const SPID* spid, time_t tmStamp);

		virtual uint32_t commandId() const {
			return 0x00000001;
		}

		virtual void serialize(StreamWriter& stream) const;

	private:
		const SPID* spid;
		time_t timeStamp;
	};

	class ConnectResponse : public comm::Arrival {
	public:
		ConnectResponse();

		bool isVerified() const { return verified; }
		const string& certification() const { return ismgCertification; }
		const string& errorMessage() const { return message; }
		int upperVersion() const { return version; }

		virtual void deserialize( StreamReader& reader );

	private:
		bool verified;
		std::string message;
		std::string ismgCertification;
		int version;
	};
}