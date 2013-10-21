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
				printf("�����쳣\n"); 
			});
		},
		180.0f
		);

	HANDLE workEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

	SPID* spid = new SPID("403037", "1234");
	bool connectSuccess = gateWay.open(spid, [spid, workEvent](bool verfied, const char* message){
		delete spid;

		msgLock.lock([verfied, message]{
			if (verfied) {
				printf("�ɹ���¼ISMG������\n");
			} else  {
				printf("ISMG��������֤ʧ��:%s\n", message);
			}
		});

		::SetEvent(workEvent);
	});

	if (connectSuccess) {
		::WaitForSingleObject(workEvent, INFINITE);

		bool toContinue = true;
		uint64_t msgId;
		while(toContinue) {
			msgLock.lock([](){
				cout<<"[1]\t����Ԥ����Ϣ"<<endl;
				cout<<"[2]\tȡ��Ԥ�ö��ŷ���"<<endl;
				cout<<"[3]\t��������ѯ"<<endl;
				cout<<"[4]\t��ҵ���ѯ"<<endl;
				cout<<"[����]\tֹͣ����"<<endl;
			});

			int choice;
			cin>>choice;
			if (choice==1) {
				MessageTask* task = new MessageTask("1234567890", "13926136535", "Hello cmpp");
				gateWay.send(task, [task, &msgId](bool accepted, const char* failure, uint64_t id) {
					delete task;
					msgId = id;
					msgLock.lock([accepted, failure, id](){
						if (accepted) {
							printf("����Ϣ�����ѱ����������ܣ����ص�MSGIDΪ:%u64\n", id);
						} else {
							printf("����Ϣ���񱻷������ܾ���ԭ����:%s\n", failure);
						}
					});
				});
			} else if (choice==2) {
				gateWay.cancel(msgId, [](bool canceled) {
					msgLock.lock([canceled]() {
						if (canceled)
							cout<<"��Ϣ���������ѱ��ɹ�ȡ��"<<endl;
						else
							cout<<"��Ϣ��������ʧ��"<<endl;
					});						
				});
			} else if (choice==3) {
				gateWay.queryForAmounts(2013, 12, 12, [](Statistics& stat) {
					msgLock.lock([]() {
						cout<<"�ɹ���ɰ�������ѯ��ҵ��"<<endl;
					});
				});
			} else if (choice==4) {
				gateWay.queryForService(2012, 10, 10, "abcdefghij", [](Statistics& stat) {
					msgLock.lock([]() {
						cout<<"�ɹ���ɰ������ѯ��ҵ��"<<endl;
					});
				});
			}
			else {
				gateWay.close();
				toContinue = false;
			}

			printf("\n");
		}
	} else {
		cout<<"�޷�����ISMG������"<<endl;
	}

	WSACleanup();
	return 0;
}

