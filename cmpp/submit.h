#pragma once
#include <string>
#include "communicator.h"

namespace cmpp {
	class MessageTask;

	class SubmitSMS : public comm::Departure {
	public:
		SubmitSMS(const MessageTask* task, const string& spid);

	protected:
		virtual uint32_t commandId() const;
		virtual void serialize(comm::StreamWriter& writer) const;

	private:
		const MessageTask* task;
		std::string spid;
	};

	class SubmitSMSResponse : public comm::Arrival {
	public:
		bool isSuccess() const;

		uint64_t messageId() const;

		const string& reason() const;

	protected:
		virtual void deserialize(comm::StreamReader& reader);

	private:
		bool success;
		uint64_t id;
		string failure;
	};
}