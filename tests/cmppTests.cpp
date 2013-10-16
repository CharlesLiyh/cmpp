#include "stdafx.h"
#include <string>
#include <vector>
#include <winsock2.h>
#include "gtest/gtest.h"
#include "cmpp/cmpp.h"
#include "cmpp/cmppsocket.h"
#include "cmpp/conceptions.h"
#include "cmpp/communicator.h"
#include "cmpp/connect.h"
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
	MessageGateway gateWay("127.0.0.1", 7890, 
		[](const char* src, const char*dst, const char* text){ printf("[SMS Report]src:%s dst:%s text:%s\n", src, dst, text); }, 
		[](uint64_t msgId, DeliverCode code){ printf("[Delivery Report]msgId:%PRIu64 code:%d\n", code);});
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

/*
TEST_F(CMPPTests, CheckBytesOfLoginPackage) {

	ActionRecorder recorder;
	MessageGateway gateWay(recorder);

	gateWay.send(Login("901001", "test", 1380813704), [](bool success, const char* failure){});

}
*/