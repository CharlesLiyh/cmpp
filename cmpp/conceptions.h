#pragma once
#include <cstdint>

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

	class CMPP_API Statistics {
	public:
		Statistics();
		~Statistics();
		void fillsWith(const char* date, const char* service, uint32_t msgAmount, uint32_t userAmount,
			uint32_t sr, uint32_t qr, uint32_t fr, uint32_t ss, uint32_t qs, uint32_t fs);
		const char* date() const;
		const char* service() const;
		uint32_t messageAmountFromSP() const;
		uint32_t userAmountFromSP() const;
		uint32_t succeedRetransmited() const;
		uint32_t queuedRetransmited() const;
		uint32_t failedRetransmited() const;
		uint32_t succeedSentForSP() const;
		uint32_t queuedSendForSP() const;
		uint32_t failedSentFoSP() const;

	private:
		const char* tillDate;
		const char* serviceCode;
		uint32_t msgAmount;
		uint32_t userAmount;
		uint32_t succeedRetrans;
		uint32_t queuedRetrans;
		uint32_t failedRetrans;
		uint32_t succeedSent;
		uint32_t queuedSent;
		uint32_t failedSent;
	};
}