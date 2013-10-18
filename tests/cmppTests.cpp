#include "stdafx.h"
#include "cmpp/cmpp.h"
#include "cmpp/conceptions.h"
#include "cmpp/communicator.h"
#include "gtest/gtest.h"
#include <windows.h>
#include <string>
#include <vector>

using namespace comm;

#pragma comment(lib, "ws2_32.lib")

using namespace std;
using namespace cmpp;
using namespace testing;

int decValFrom(char letter) {
	letter = toupper(letter);
	if (letter>='0' && letter<='9')
		return letter - '0';
	else
		return letter - 'A' + 10;
}

vector<uint8_t> hexBytesFrom(const char* str) {
	const char* end = str + strlen(str);

	vector<uint8_t> bytes;
	while(str<end) {
		bytes.push_back(decValFrom(str[0])<<4 | decValFrom(str[1]));

		str += 3;
	}

	return bytes;
}

class CMPPTests : public Test {
};

TEST_F(CMPPTests, aTest) {
	Communicator commObj("127.0.0.1", 7890);
	MessageGateway gateWay(&commObj, 
		[](const char* src, const char*dst, const char* text){ printf("[SMS Report]src:%s dst:%s text:%s\n", src, dst, text); }, 
		[](uint64_t msgId, DeliverCode code){ printf("[Delivery Report]msgId:%PRIu64 code:%d\n", code);}, 3, 60, 3 );
	SPID* spid = new SPID("403037", "1234");
	gateWay.open(spid, [spid](bool verfied, const char* message){
		delete spid;
	});

	MessageTask* task = new MessageTask("1234567890", "13926136535", "Hello cmpp");
	gateWay.send(task, [task](bool accepted, const char* failure, uint64_t id) {
		delete task;
	});

	HANDLE e = CreateEvent(NULL, TRUE, FALSE, NULL);
	WaitForSingleObject(e, INFINITE);
	gateWay.close();
}