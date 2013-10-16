#pragma once

#include "stdafx.h"
#include <winsock2.h>
#include <queue>
#include "cmppdefines.h"
#include "sgip.h"

const int nCMPP_RCV_WINDOW_SIZE  =2048 ;          //  接收窗口大小

class CcmppSocket
{
public:
	CcmppSocket ();
	~CcmppSocket();

public:
	int		init( char *spid, char *passwd, char *ismg, unsigned short port = 7890);
	int		Submit( CMPP_SUBMIT &msg, int &iseqid, DWORD dwMilliseconds = INFINITE);
	void	Uninit();
	std::queue<CMPP_DELIVER> _queueDeliver;
	std::queue<CMPP_SUBMIT_RESP_EX> _queueSubmitResp;
	static	int _iActivetest;

private:
	int		_connect( char *ismg, unsigned short port);
	int		_login	( char *spid, char *passwd);
	void	_logout ();					//	注销
	void	_exit	();					//	清除工作线程，退出接口
	static	DWORD	WINAPI	thread_send( LPVOID pdata);
	static	DWORD	WINAPI	thread_recv( LPVOID pdata);
	static	DWORD	WINAPI	thread_actv( LPVOID pdata);
private:
	char*	_timestamp( char *buf);		//	生成10位的时间戳
	__int64	_ntoh64	( __int64 inval);	//	将64位的整数转成网络字节顺序
	__int64	_hton64	( __int64 inval);	//	将64位的整数转成主机字节顺序
	int		_getseqid();				//	得到下一个数据包的流水号
	int		_send( char *buf, int len); //	同步发送数据
	int		_recv( char *buf, int len); //	同步接收数据

private:
	SOCKET	_soc;					//	连接服务器的套接字
	bool	_binitialized;			//	接口初始化标志
	bool	_bexitting;				//	请求线程退出
	char	_spid[10];				//	6 位企业代码
	char	_ismg[20];				//	短信网关IP地址
	char	_passwd[20];			//	企业登陆短信网关的口令
	unsigned short _port;			//	短信网关的端口，默认7890

	int		_seqid;					//	流水号，需同步
	HANDLE	_hsend;					//	发送线程
	HANDLE	_hrecv;					//	接收线程
	HANDLE	_hactv;					//	链路维持线程
	HANDLE	_hguard;				//	守卫线程，负责自动重连
	HANDLE	_hsema_wnd;				//	计数数据窗口的空格
	HANDLE	_hevnt_data;			//	计数新提交的数据报
    HANDLE  _hsema_rev;             //  数据接收窗口空余大小

//	HANDLE  _hdeliver;				// 上行事件通知

	CRITICAL_SECTION	_csec_wnd;	//	同步数据窗口
	CRITICAL_SECTION	_csec_snd;	//	同步发送数据
	CRITICAL_SECTION	_csec_recv;	//	同步接收数据
	CRITICAL_SECTION	_csec_seq;	//	同步流水号
    CRITICAL_SECTION    _csec_revwnd; //  同步数据接收窗口

	CMPP_PACKAGE	_window[nCMPP_WINDOW_SIZE];	//	数据窗口，需同步
};