#include "stdafx.h"
#include <windows.h>

int _tmain(int argc, _TCHAR* argv[]) {
	HANDLE stopEvent = ::OpenEventA(EVENT_ALL_ACCESS, FALSE, "cmpp.stopTests");
	if (stopEvent!=NULL) {
		::SetEvent(stopEvent);
		printf("Done");
	}
	else {
		printf("Can't find cmpp tests");
	}

	printf("\n");
	return 0;
}

