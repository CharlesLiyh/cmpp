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
}