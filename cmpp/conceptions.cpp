#include "stdafx.h"
#include "conceptions.h"
#include <string>

namespace cmpp {
	//////////////////////////////////////////////////////////////////////////
	// SPID
	SPID::SPID(const char* userName, const char* password) {
		name = _strdup(userName);
		pwd = _strdup(password);
	}

	SPID::~SPID() {
		delete name;
		delete pwd;
	}

	const char* SPID::username() const {
		return name;
	}

	const char* SPID::password() const {
		return pwd;
	}

	//////////////////////////////////////////////////////////////////////////
	// MessageTask
	MessageTask::MessageTask(const char* srcNumber, const char* destNumbers, const char* messageText) {
		src = _strdup(srcNumber);
		dest = _strdup(destNumbers);
		text = _strdup(messageText);
	}

	MessageTask::~MessageTask(void) {
		delete src;
		delete dest;
		delete text;
	}

	const char* MessageTask::srcNumber() const {
		return src;
	}

	const char* MessageTask::destNumbers() const {
		return dest;
	}

	const char* MessageTask::messageText() const {
		return text;
	}

	int MessageTask::textLength() const {
		return strlen(text);
	}

	//////////////////////////////////////////////////////////////////////////
	// Statistics

	Statistics::Statistics() {
		tillDate = nullptr;
		serviceCode = nullptr;
	}

	Statistics::~Statistics() {
		delete tillDate;
		delete serviceCode;
	}

	void Statistics::fillsWith( const char* date, const char* service, uint32_t mAmount, uint32_t uAmount, uint32_t sr, uint32_t qr, uint32_t fr, uint32_t ss, uint32_t qs, uint32_t fs ) {
		tillDate = _strdup(date);
		serviceCode = _strdup(service);
		msgAmount = mAmount;
		userAmount = uAmount;
		succeedRetrans = sr;
		queuedRetrans = qr;
		failedRetrans = fr;
		succeedSent = ss;
		queuedSent = qs;
		failedSent = fs;
	}

	uint32_t Statistics::messageAmountFromSP() const {
		return msgAmount;
	}

	uint32_t Statistics::userAmountFromSP() const {
		return userAmount;
	}

	uint32_t Statistics::succeedRetransmited() const {
		return succeedRetrans;
	}

	uint32_t Statistics::queuedRetransmited() const {
		return queuedRetrans;
	}

	uint32_t Statistics::failedRetransmited() const {
		return failedRetrans;
	}

	uint32_t Statistics::succeedSentForSP() const {
		return succeedSent;
	}

	uint32_t Statistics::queuedSendForSP() const {
		return queuedSent;
	}

	uint32_t Statistics::failedSentFoSP() const {
		return failedSent;
	}


}