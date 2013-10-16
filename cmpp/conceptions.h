#pragma once

#ifdef CMPP_EXPORTS
#define CMPP_API __declspec(dllexport)
#else
#define CMPP_API __declspec(dllimport)
#endif

namespace cmpp {
	enum DeliverCode {
		Delivered,
		Expired,
		Deleted,
		Undeliverable,
		Accepted,
		Unknown,
		Rejected,
	};

	class CMPP_API SPID {
	public:
		SPID(const char* userName, const char* password);
		~SPID();

		const char* username() const;
		const char* password() const;

	private:
		const char* name;
		const char* pwd;
	};

	class CMPP_API MessageTask {
	public:
		MessageTask(const char* srcNumber, const char* destNumbers, const char* messageText);
		~MessageTask(void);

		const char* srcNumber() const;

		const char* destNumbers() const;

		const char* messageText() const;
		int textLength() const;
	private:
		const char* src;
		const char* dest;
		const char* text;
	};
}