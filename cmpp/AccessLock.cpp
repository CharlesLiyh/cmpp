#include "stdafx.h"
#include "AccessLock.h"

AccessLock::AccessLock()
{
	::InitializeCriticalSection(&section);
}

AccessLock::~AccessLock()
{
	::DeleteCriticalSection(&section);
}

void AccessLock::lock( function<void()> action )
{
	EnterCriticalSection(&section);
	action();
	LeaveCriticalSection(&section);
}
