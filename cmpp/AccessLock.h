#pragma once
#include <functional>
#include <windows.h>
using std::function;

class AccessLock {
public:
	AccessLock();
	~AccessLock();

	void lock(function<void()> action);

private:
	CRITICAL_SECTION section;
};

