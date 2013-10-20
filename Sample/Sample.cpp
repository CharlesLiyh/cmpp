#include "stdafx.h"
#include <winsock2.h>
#include "cmpp\Protocol20.h"
#include "cmpp\cmpp.h"
#include "cmpp\AccessLock.h"
#include <iostream>

using namespace std;
using namespace cmpp;

#pragma comment(lib, "ws2_32.lib")

AccessLock msgLock;

int _tmain(int argc, _TCHAR* argv[]) {
	WSADATA wsaData;
	WSAStartup( MAKEWORD( 2, 2 ), &wsaData );

	Protocol20 v20("127.0.0.1", 7890);
	MessageGateway gateWay(&v20, 
		[](const char* src, const char*dst, const char* text){ 
			msgLock.lock([src, dst, text]{
				printf("[SMS Report]src:%s dst:%s text:%s\n", src, dst, text); 
			});
		}, 
		[](uint64_t msgId, DeliverCode code){
			msgLock.lock([code]{
				printf("[Delivery Report]msgId:%PRIu64 code:%d\n", code);
			});
		},
		[](const char* message) {
			msgLock.lock([]{
				printf("发生异常\n"); 
			});
		},
		3);

	HANDLE workEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

	SPID* spid = new SPID("403037", "1234");
	bool connectSuccess = gateWay.open(spid, [spid, workEvent](bool verfied, const char* message){
		delete spid;

		msgLock.lock([verfied, message]{
			if (verfied) {
				printf("成功登录ISMG服务器\n");
			} else  {
				printf("ISMG服务器验证失败:%s\n", message);
			}
		});

		::SetEvent(workEvent);
	});

	if (connectSuccess) {
		::WaitForSingleObject(workEvent, INFINITE);

		bool toContinue = true;
		while(toContinue) {
			msgLock.lock([](){
				cout<<"[1]    发送预置消息"<<endl;
				cout<<"[其它] 停止服务"<<endl;
			});

			int choice;
			cin>>choice;
			if (choice==1) {
				MessageTask* task = new MessageTask("1234567890", "13926136535", "Hello cmpp");
				gateWay.send(task, [task](bool accepted, const char* failure, uint64_t id) {
					delete task;
					msgLock.lock([accepted, failure, id](){
						if (accepted) {
							printf("短消息任务已被服务器接受，返回的MSGID为:%u64\n", id);
						} else {
							printf("短消息任务被服务器拒绝，原因是:%s\n", failure);
						}
					});
				});
			} else {
				gateWay.close();
				toContinue = false;
			}

			printf("\n");
		}
	} else {
		cout<<"无法连接ISMG服务器"<<endl;
	}

	WSACleanup();
	return 0;
}

