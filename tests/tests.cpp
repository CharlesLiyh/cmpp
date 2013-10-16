#include "stdafx.h"
#include "gtest/gtest.h"
#include <winsock2.h>

GTEST_API_ int main(int argc, char **argv) {
	WSADATA wsaData;
	WSAStartup( MAKEWORD( 2, 2 ), &wsaData );

	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}