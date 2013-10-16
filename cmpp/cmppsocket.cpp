#include "stdafx.h"
#include <stdlib.h>
#include "cmppsocket.h"

#ifdef _DEBUG_EX
#include <iterator>
#include <list>
#include <string>
#include <iostream>
#include <fstream>
#include <ostream>
#endif

#include <time.h>
#include "md5.h"

#pragma comment(lib, "ws2_32.lib")

//将接收的数据dump 到 _revdebug[]

#ifdef _DEBUG_EX
using namespace std;
char _revdebug[1024*1024];
char * _revp=_revdebug;
std::list<std::string> _dbgeventlst;
char _dbgtemp[420];

std::ostream& operator<<(std::ostream& out, const std::list<std::string>& l)
{
	std::copy(l.begin(), l.end(),
		ostream_iterator<std::string,char> (out,"\n"));
	return out;
}
#endif

int	CcmppSocket::_iActivetest = 0;
CcmppSocket::CcmppSocket()
{
#ifdef _DEBUG_EX
	_dbgeventlst.push_back("Begin Class CcmppSocket Constructor.");
#endif

	//	初始化私有变量
	_binitialized = false;
	_bexitting = false;
	_seqid	= 0;
	memset( (void *)_window, 0, sizeof( _window));
	InitializeCriticalSection( &_csec_wnd);
	InitializeCriticalSection( &_csec_snd);
	InitializeCriticalSection( &_csec_recv);
	InitializeCriticalSection( &_csec_seq);
	_hsema_wnd = CreateSemaphore(			//	创建计数信号量
		NULL,
		nCMPP_WINDOW_SIZE,
		nCMPP_WINDOW_SIZE,
		NULL);
	_hevnt_data = CreateEvent(				//	创建提示发送的事件
		NULL,
		false,
		false,
		NULL);
	/*	_hdeliver = CreateEvent(				//	创建上行事件
	NULL,
	false,
	false,
	NULL);*/
	//	初始化网络
	WSADATA wsaData;
	WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
#ifdef _DEBUG_EX
	_dbgeventlst.push_back("End   Class CcmppSocket Constructor.");
#endif
}

CcmppSocket::~CcmppSocket()
{
	//	发送CMPP_TERMINATE，从服务器注销
#ifdef _DEBUG_EX
	_dbgeventlst.push_back("Begin Class CcmppSocket Desstructor.");
#endif
	if( _binitialized)
		_exit();
	WSACleanup();
	//	清除临界区等
	DeleteCriticalSection( &_csec_wnd);
	DeleteCriticalSection( &_csec_snd);
	DeleteCriticalSection( &_csec_recv);
	DeleteCriticalSection( &_csec_seq);
	CloseHandle( _hsema_wnd);
	CloseHandle( _hevnt_data);
	//	CloseHandle( _hdeliver);
#ifdef _DEBUG_EX
	_dbgeventlst.push_back("Begin Class CcmppSocket Desstructor.");
#endif
}

/******************************************************************************
spid		企业代码		例如：901001
passwd	登陆口令		例如：test
ismg		短信网关的地址，例如：127.0.0.1
port		短信网关的端口，例如：7890
******************************************************************************/
int CcmppSocket::init( char *spid, char *passwd, char *ismg, unsigned short port)
{
	if( _binitialized)
		_exit();
	int err;
	err = _connect( ismg, port);
	/*
	if( err != 0)
	{
		return eCMPP_INIT_CONNECT;
	}
	*/

	err = _login( spid, passwd);
	if( err != 0)
	{
#ifdef _DEBUG_EX
		sprintf(_dbgtemp,"    Connect To GateWay Server Fail Return Code=%d Error Code=%d.",err,WSAGetLastError());
		_dbgeventlst.push_back(_dbgtemp);
#endif
		closesocket( _soc);
		return err;
	}
#ifdef _DEBUG_EX
	_dbgeventlst.push_back("Login To GateWay Server Success.");
#endif
	//	保存配置，以备后用
	strcpy( _spid,	spid);
	strcpy( _passwd,passwd);
	strcpy( _ismg,	ismg);
	_port = port;
	//	启动发送、接收数据的线程
#ifdef _DEBUG_EX
	_dbgeventlst.push_back("Begin Thread Send SMS.");
#endif
	_hsend = CreateThread(	//	短信发送线程
		NULL,
		NULL,
		thread_send,
		(LPVOID)this,
		0,
		NULL);
#ifdef _DEBUG_EX
	_dbgeventlst.push_back("Begin Thread Recieve SMS.");
#endif
	_hrecv = CreateThread(	//	短信接收线程
		NULL,
		NULL,
		thread_recv,
		(LPVOID)this,
		0,
		NULL);
#ifdef _DEBUG_EX
	_dbgeventlst.push_back("Begin Thread Activate.");
#endif
	_hactv = CreateThread(	//	链路检查
		NULL,
		NULL,
		thread_actv,
		(LPVOID)this,
		0,
		NULL);
	//	初始化成功，设置成功标志
	_binitialized = true;
	//	放弃当前时间片
	Sleep( 0);

#ifdef _DEBUG_EX
	_dbgeventlst.push_back("Connection Initializtion Success.");
#endif
	return eCMPP_INIT_OK;
}

/******************************************************************************
msg				向服务器提交的数据
dwMilliseconds	在成功的将数据插入数据窗口前等待的时间

返回值			描述
===============	==============================		
0					成功
eCMPP_NEED_INIT	接口未初始化
WAIT_TIMEOUT		操作超时
WAIT_ABANDONED	工作线程异常退出，可能是网络故障
******************************************************************************/
int	CcmppSocket::Submit( CMPP_SUBMIT &msg, int &iseqid, DWORD dwMilliseconds)
{
	CMPP_PACKAGE	pkg;
	CMPP_HEAD		&head = (CMPP_HEAD		&)pkg.head;
	int	err, nsize;

	if(!_binitialized)
		return eCMPP_NEED_INIT;

	pkg.n = 0;							//	发送失败，则重发两次
	pkg.t = time( NULL);				//	立即发送

	nsize = sizeof( CMPP_HEAD) + sizeof( CMPP_SUBMIT);
	//	因为CMPP协议中包的长度是可变的，而我定义的数据结构中
	//	包的长度采用的是最大长度，所以这里需要修正
	nsize = nsize + msg.msglen - sizeof( msg.msgcontent);
	head.size = htonl( nsize);
	head.cmdid= htonl( nCMPP_SUBMIT);
	head.seqid= htonl( _getseqid());
	iseqid =  ntohl (head.seqid); //返回seqid做状态返回处理

	memcpy( (void *)pkg.data, (void *)&msg, sizeof( msg));
	//	将最后8个字节的保留数据拷贝到适当的位置
	memcpy(
		(void *)(pkg.data + nsize - sizeof( msg.reserve) - sizeof( CMPP_HEAD)),
		(void *)msg.reserve,
		sizeof( msg.reserve));
	//	等候数据发送窗口的空位
	err = WaitForSingleObject( _hsema_wnd, dwMilliseconds);
	//	等待超时或程序异常
	if( err != WAIT_OBJECT_0)
		return err;
	//	申请数据发送窗口的使用权
	EnterCriticalSection( &_csec_wnd);

	int i;
	for(i=0; i<nCMPP_WINDOW_SIZE; i++)
	{
		//	找到一个空位
		if( _window[i].head.cmdid == 0)
			break;
	}
	memcpy( (void *)&_window[i], (void *)&pkg, sizeof( pkg));
	//	_window[i].n = 0;

	LeaveCriticalSection( &_csec_wnd);
	//	唤醒数据发送线程
	PulseEvent( _hevnt_data);

	return 0;
}

int CcmppSocket::_connect( char *ismg, unsigned short port)
{
	int err;
	struct sockaddr_in addr;

	_soc = socket( AF_INET, SOCK_STREAM, 0);

	addr.sin_family = AF_INET;
	addr.sin_port   = htons( port);
	addr.sin_addr.s_addr   = inet_addr( ismg);

	err = connect( _soc, (struct sockaddr *)&addr, sizeof( addr));
	return err;
}

int CcmppSocket::_login( char *spid, char *passwd)
{
#ifdef _DEBUG_EX
	sprintf(_dbgtemp,"_login begin ismg=%s,passwd=%s", spid, passwd );
	_dbgeventlst.push_back(_dbgtemp);
#endif

	CMPP_PACKAGE	pkg;
	CMPP_CONNECT	&msg = *(CMPP_CONNECT	*)pkg.data;
	int err, nsize;

	MD5 ctx;
	char authsrc[50], *pos, timestr[20];

	memset( (void *)&msg, 0, sizeof(msg));

	nsize = sizeof( pkg.head) + sizeof(msg);
	pkg.head.size  = htonl( nsize);
	pkg.head.cmdid = htonl( nCMPP_CONNECT);
	pkg.head.seqid = htonl( _getseqid());

	strcpy( (char *)msg.spid, spid);
	//	计算单向HASH函数的值
	memset( authsrc, 0, sizeof( authsrc));
	strcpy( authsrc, spid);
	pos = authsrc + strlen( spid) + 9;
	strcpy( (char *)pos, passwd);
	pos += strlen( passwd);
	strcpy( pos, _timestamp( timestr ));
	pos += strlen( timestr);

	ctx.update( (unsigned char *)authsrc, (int)(pos - authsrc) );
	ctx.finalize();
	ctx.raw_digest( msg.digest);

	msg.ver = nCMPP_VERSION;
	msg.timestamp = htonl( atol( timestr));
	//	发送身份验证数据
	err = _send( (char *)&pkg, nsize);
	if( err != nsize)
		return eCMPP_INIT_CONNECT;

#ifdef _DEBUG_EX
	sprintf(_dbgtemp,"msg digest=%s\n", msg.digest );
	_dbgeventlst.push_back(_dbgtemp);
	sprintf(_dbgtemp,"msg spid=%s\n", msg.spid );
	_dbgeventlst.push_back(_dbgtemp);
	sprintf(_dbgtemp,"msg timestamp=%d\n", msg.timestamp );
	_dbgeventlst.push_back(_dbgtemp);
#endif
	//	接收返回的数据包
	CMPP_CONNECT_RESP &msgresp = *(CMPP_CONNECT_RESP *)pkg.data;
	nsize = sizeof( CMPP_HEAD) + sizeof( CMPP_CONNECT_RESP);

	err = _recv( (char *)&pkg, nsize);
	if( err != nsize )
	{
#ifdef _DEBUG_EX
		sprintf(_dbgtemp,"recv CMPP_CONNECT_RESP fail err=%d, data =%s", err, (char *)&pkg );
		_dbgeventlst.push_back(_dbgtemp);
#endif
		return eCMPP_INIT_CONNECT;
	}

#ifdef _DEBUG_EX
	sprintf(_dbgtemp,"recv CMPP_CONNECT_RESP status=%d", msgresp.status );
	_dbgeventlst.push_back(_dbgtemp);
#endif

	return ntohl( msgresp.status);
}

void CcmppSocket::_exit()
{
	HANDLE	threads[] = { _hsend, _hrecv, _hactv};
	int		i,
		nthreads;
	//	请求工作线程退出
	_bexitting = true;

	Sleep( 1000);
	//	关闭请求，强制结束所有尚未退出的线程
	_bexitting = false;

	nthreads = sizeof( threads) / sizeof( HANDLE);
	for( i=0; i<nthreads; i++)
	{
		TerminateThread( threads[i], 0);
		CloseHandle( threads[i]);
	}
	//	注销
	_logout();
	closesocket( _soc);
}

void CcmppSocket::_logout()
{
	CMPP_HEAD	msg;
	int			err, nsize;

	nsize = sizeof( msg);
	msg.size  = htonl( nsize);
	msg.cmdid = htonl( nCMPP_TERMINATE);
	msg.seqid = htonl( _getseqid());

	err = _send( (char *)&msg, sizeof( msg));
	if( err != nsize)
		return;

	err = _recv( (char *)&msg, sizeof( msg));

#ifdef _DEBUG_EX
	_dbgeventlst.push_back(" cmppsocket logout");
#endif
	return;
}

void CcmppSocket::Uninit()
{
	_exit();
}

/******************************************************************************
数据发送线程

对于新提交的数据报，立即发送
超过60秒未收到回应，则重发
******************************************************************************/
DWORD	WINAPI	CcmppSocket::thread_send( LPVOID pdata)
{
	CcmppSocket		&cmpp = *( CcmppSocket *)pdata;
	CMPP_PACKAGE	window[nCMPP_WINDOW_SIZE];
	int	i;
	int	err;
	int	nsize;
	int	dwMilliseconds = 1000;		//	轮询间隔为1000毫秒
	for( ;;)
	{
		//	轮询间隔1秒
		err = WaitForSingleObject(
			cmpp._hevnt_data,
			dwMilliseconds);
		//	出错了，结束线程
		if( err == WAIT_FAILED)
			break;
		//	申请数据发送窗口的使用权
		EnterCriticalSection( &cmpp._csec_wnd);
		//	拷贝数据到自己的内存区域，避免在临界区中发送数据
		memcpy( (void *)window, (void *)cmpp._window, sizeof( window));
		//	搜寻并修改到期时间
		for( i=0; i<nCMPP_WINDOW_SIZE; i++)
		{
			//	空位，跳过
			if( cmpp._window[i].head.cmdid == 0)
				continue;
			//	未到发送时间，跳过
			if( cmpp._window[i].t > time( NULL))
				continue;
			//	设置下一次发送的时间，当前时间＋60秒
			cmpp._window[i].t += 60;

			//  如果发送次数超过3次，从队列中清除
			if( cmpp._window[i].n >= 3 )
			{
				long prevcount;
				cmpp._window[i].head.cmdid = 0;
				ReleaseSemaphore( cmpp._hsema_wnd, 1, &prevcount);
				continue;
			}

			cmpp._window[i].n++;
		}
		LeaveCriticalSection( &cmpp._csec_wnd);
		//	搜寻并发送到期的数据
		for( i=0; i<nCMPP_WINDOW_SIZE; i++)
		{
			//	空位，跳过
			if( window[i].head.cmdid == 0)
				continue;
			//	未到发送时间，跳过
			if( window[i].t > time( NULL))
				continue;

			if( window[i].n >= 3 )
				continue;

			//	发送
			nsize = ntohl( window[i].head.size);
			err = cmpp._send( (char *)&window[i], nsize);
#ifdef _DEBUG_EX
			sprintf(_dbgtemp,"Thread send CMPP_SUBMIT seqid=%d,n=%d, ret size = %d", ntohl(window[i].head.seqid) ,window[i].n, err);
			_dbgeventlst.push_back(_dbgtemp);
			CMPP_SUBMIT & msg=*(CMPP_SUBMIT *) &window[i].data;
			sprintf(_dbgtemp,"destnumbers = %s", msg.destnumbers );
			_dbgeventlst.push_back(_dbgtemp);
			sprintf(_dbgtemp,"msgcontent = %s", msg.msgcontent );
			_dbgeventlst.push_back(_dbgtemp);
			sprintf(_dbgtemp,"msgsrc = %s", msg.msgsrc );
			_dbgeventlst.push_back(_dbgtemp);
			sprintf(_dbgtemp,"srcnumber = %s", msg.srcnumber );
			_dbgeventlst.push_back(_dbgtemp);
#endif
			//	发送出错，结束线程
			if( err != nsize)
				return 0;
		}
		//	主线程请求退出
		if( cmpp._bexitting)
			break;
	}
	return 0;
}

DWORD	WINAPI	CcmppSocket::thread_recv( LPVOID pdata)
{
	int		err, i;
	long	prevcount;
	FD_SET	fdset;
	TIMEVAL	timeout;
	CcmppSocket		&cmpp = *( CcmppSocket *)pdata;
	CMPP_PACKAGE	pkg;
	memset((void *)&pkg,0,sizeof(pkg));

	FD_ZERO( &fdset);
	FD_SET( cmpp._soc, &fdset);
	//	轮询间隔1秒
	timeout.tv_sec = 1;
	timeout.tv_usec= 0;
#ifdef _DEBUG_EX
	_dbgeventlst.push_back("Thread Recieve : Thread Begin.");
#endif

	for( ;;)
	{
		FD_ZERO( &fdset);
		FD_SET( cmpp._soc, &fdset);
		err = select(
			0,
			&fdset,
			NULL,
			NULL,
			&timeout);
		//	出错
		if( err == SOCKET_ERROR)
		{
#ifdef _DEBUG_EX
			sprintf(_dbgtemp,"Thread Recieve : Socket Wait for Data Error.Error Code=%d",WSAGetLastError());
			_dbgeventlst.push_back(_dbgtemp);
#endif
			continue;//break;
		}
		//	超时
		if( err == 0)
			continue;
		//	先接收包头部分，以确定包的大小、类型
		err = cmpp._recv( (char *)&pkg.head, sizeof( pkg.head));
		if( err != sizeof( pkg.head))
		{
#ifdef _DEBUG_EX
			sprintf(_dbgtemp,"Thread Recieve : Recieve Header Return Code=%d Needed Length=%d Error=%d.",err,sizeof( pkg.head),WSAGetLastError());
			_dbgeventlst.push_back(_dbgtemp);
#endif
			continue;//break;
		}
		//	接收包体
		err = cmpp._recv( pkg.data, ntohl( pkg.head.size )-sizeof( pkg.head));
		if( err == SOCKET_ERROR)
		{
#ifdef _DEBUG_EX
			sprintf(_dbgtemp,"Thread Recieve : Socket Wait for Data Error.Error Code=%d",WSAGetLastError());
			_dbgeventlst.push_back(_dbgtemp);
#endif
			continue;//break;
		}
		switch (ntohl(pkg.head.cmdid))
		{
		case nCMPP_SUBMIT_RESP:
			{
				CMPP_SUBMIT_RESP & _smtrsp=*(CMPP_SUBMIT_RESP *)&pkg.data;
				if (err!=sizeof(CMPP_SUBMIT_RESP))
				{
#ifdef _DEBUG_EX
					sprintf(_dbgtemp,"Thread Recieve : Data Body Length Error.Length=%d",err);
					_dbgeventlst.push_back(_dbgtemp);
#endif
					break;
				}
				//	申请数据发送窗口的使用权
				EnterCriticalSection( &cmpp._csec_wnd);
				//	删除对应流水号的包
				for( i=0; i<nCMPP_WINDOW_SIZE; i++)
				{
					if( cmpp._window[i].head.cmdid == 0)
						continue;
					if( cmpp._window[i].head.seqid == pkg.head.seqid)
					{
						cmpp._window[i].head.cmdid = 0;
						break;
					}
				}
				LeaveCriticalSection( &cmpp._csec_wnd);
				//	释放一个窗口格子
				ReleaseSemaphore(
					cmpp._hsema_wnd,
					1,
					&prevcount);
#ifdef _DEBUG_EX
				sprintf(_dbgtemp,"Thread Recieve : Recieve CMPP_SUBMIT_RESP CMD. SeqID=%d",ntohl(pkg.head.seqid));
				_dbgeventlst.push_back(_dbgtemp);
#endif
				CMPP_SUBMIT_RESP_EX respEx;
				respEx.msgid = _smtrsp.msgid;
				respEx.result = _smtrsp.result;
				respEx.seqid = pkg.head.seqid;

				cmpp._queueSubmitResp.push( respEx);
				break;
			}
			//2. CMPP_DELIVER
		case nCMPP_DELIVER:
			{
				CMPP_DELIVER & _dlv=*(CMPP_DELIVER *) &pkg.data;
				printf("\nRecieve Data.\n");
				memcpy((void *)_dlv.LinkID,(void *)(_dlv.msgcontent+_dlv.msglen),20);
				_dlv.msgcontent[_dlv.msglen]=0x0;
				_dlv.LinkID[20]='\0';
#ifdef _DEBUG_EX
				sprintf(_dbgtemp,"Thread Recieve : Recieve CMPP_DELIVER CMD. SeqID=%d",ntohl(pkg.head.seqid));
				_dbgeventlst.push_back(_dbgtemp);
				sprintf(_dbgtemp,"  Size: %d\n",ntohl(pkg.head.size));
				_dbgeventlst.push_back(_dbgtemp);
				sprintf(_dbgtemp,"  cmdid: %d\n",ntohl(pkg.head.cmdid));
				_dbgeventlst.push_back(_dbgtemp);
				sprintf(_dbgtemp,"  seqid: %d\n",ntohl(pkg.head.seqid));
				_dbgeventlst.push_back(_dbgtemp);
				sprintf(_dbgtemp,"  msgid: %LX\n",_dlv.msgid);
				_dbgeventlst.push_back(_dbgtemp);
				sprintf(_dbgtemp,"  destnumber: %s\n",_dlv.destnumber);
				_dbgeventlst.push_back(_dbgtemp);
				sprintf(_dbgtemp,"  serviceid : %s\n",_dlv.serviceid );
				_dbgeventlst.push_back(_dbgtemp);
				sprintf(_dbgtemp,"  tppid: %d\n",_dlv.tppid);
				_dbgeventlst.push_back(_dbgtemp);
				sprintf(_dbgtemp,"  tpudhi: %d\n",_dlv.tpudhi);
				_dbgeventlst.push_back(_dbgtemp);
				sprintf(_dbgtemp,"  msgfmt: %d\n",_dlv.msgfmt);
				_dbgeventlst.push_back(_dbgtemp);
				sprintf(_dbgtemp,"  srcnumber : %s\n",_dlv.srcnumber );
				_dbgeventlst.push_back(_dbgtemp);
				sprintf(_dbgtemp,"  srctype;: %d\n",_dlv.srctype);
				_dbgeventlst.push_back(_dbgtemp);
				sprintf(_dbgtemp,"  delivery: %d\n",_dlv.delivery);
				_dbgeventlst.push_back(_dbgtemp);
				sprintf(_dbgtemp,"  msglen;: %d\n",_dlv.msglen);
				_dbgeventlst.push_back(_dbgtemp);
				sprintf(_dbgtemp,"  msgcontent: %s\n",_dlv.msgcontent);
				_dbgeventlst.push_back(_dbgtemp);
				sprintf(_dbgtemp,"  LinkID    : %s\n",_dlv.LinkID    );
				_dbgeventlst.push_back(_dbgtemp);
#endif

				CMPP_PACKAGE	pkgResp;
				CMPP_HEAD		&head = (CMPP_HEAD		&)pkgResp.head;
				int	nsize;

				nsize = sizeof( CMPP_HEAD) + sizeof( CMPP_DELIVER_RESP);
				head.size = htonl( nsize);
				head.cmdid= htonl( nCMPP_DELIVER_RESP);
				head.seqid= pkg.head.seqid;

				CMPP_DELIVER_RESP	&resp = (CMPP_DELIVER_RESP	&)pkgResp.data;
				resp.msgid = _dlv.msgid;
				resp.result = 0;
				cmpp._send( (char *)&pkgResp, nsize);

				cmpp._queueDeliver.push(*(CMPP_DELIVER *) &pkg.data );
				//	PulseEvent( cmpp._hdeliver);
				break;
			}
			//3. CMPP_ACTIVE_TEST_RESP
		case nCMPP_ACTIVE_TEST_RESP:
			{
				_iActivetest = 0;
#ifdef _DEBUG_EX
				sprintf(_dbgtemp,"Thread Recieve : Recieve CMPP_ACTIVE_TEST_RESP CMD. SeqID=%d",ntohl(pkg.head.seqid));
				_dbgeventlst.push_back(_dbgtemp);
#endif
				break;
			}
		default:
			{
#ifdef _DEBUG_EX
				sprintf(_dbgtemp,"Thread Recieve : Recieve CMPP_ACTIVE_TEST_RESP CMD. CMD=%LX SeqID=%d",ntohl(pkg.head.cmdid),ntohl(pkg.head.seqid));
				_dbgeventlst.push_back(_dbgtemp);
#endif

			}
		}
		//	主线程请求退出
		if( cmpp._bexitting)
			break;
	}
#ifdef _DEBUG_EX
	_dbgeventlst.push_back("Thread Recieve : Thread End.");
#endif

	return 0;

}

/******************************************************************************
*    数据接收线程，从端口向数据窗口填充数据
******************************************************************************/
/*
DWORD WINAPI CcmppSocket::thread_revwnd(LPVOID LParam)
{
/*
int     err=0;
char    buff=0;
for(;;)
{
err=WaitForSingleObject(_hsema_rev,1000);
if (err==WAIT_OBJECT_0)
{
err=_recv(&buff,1);
if (err!=SOCKET_ERROR)
{
EnterCriticalSection(_csec_revwnd);
_revbuff[_prevbuff++]=buff;
LeaveCriticalSection(_csec_revwnd);
}
}
if( cmpp._bexitting)
break;
}

}
*/
/******************************************************************************
*	为了保证线程及时正常退出，设定轮询间隔为1秒
*	CMPP3.0建议的链路检测间隔为180秒
*	为了兼容各个厂家的实现，设为30秒
******************************************************************************/
DWORD	WINAPI	CcmppSocket::thread_actv( LPVOID pdata)
{
	CcmppSocket &cmpp = *( CcmppSocket *)pdata;
	CMPP_HEAD	msg;
	int			c = 0,		//	计数
		n = 30;		//	等待的秒数
	for( ;;)
	{
		Sleep( 1000);
		//	主线程请求退出
		if( cmpp._bexitting)
			break;
		if( ++ c < n)
			continue;
		c = 0;
		//	获得数据发送权利，发送链路测试包
		msg.size = htonl( sizeof( msg));
		msg.cmdid= htonl( nCMPP_ACTIVE_TEST);
		msg.seqid= htonl( cmpp._getseqid());

		if ( _iActivetest >= 3 ) 
			break;

		cmpp._send( (char *)&msg, sizeof( msg));
		_iActivetest++;
	}

	//	if ( _iActivetest >= 3 ) 
	//	{
#ifdef _DEBUG_EX
	_dbgeventlst.push_back("thread_actv exit .");
#endif
	cmpp._bexitting = true;
	//		init(_spid, _passwd, _ismg);
	//	}
	return 0;
}

char * CcmppSocket::_timestamp( char *buf)
{
	time_t tt;
	struct tm *now;

	time( &tt);
	now = localtime( &tt);
	sprintf( buf, "%02d%02d%02d%02d%02d",
		now->tm_mon + 1,
		now->tm_mday,
		now->tm_hour,
		now->tm_min,
		now->tm_sec);
	return buf;
}

__int64	CcmppSocket::_ntoh64( __int64 inval)
{
	__int64 outval = 0;
	for( int i=0; i<8; i++)
		outval = ( outval << 8) + ( ( inval >> ( i * 8)) & 255);

	return outval;
}

__int64	CcmppSocket::_hton64( __int64 inval)
{
	return _ntoh64( inval);
}

int	CcmppSocket::_getseqid()
{
	int id;
	EnterCriticalSection( &_csec_seq);
	id = ++_seqid;
	LeaveCriticalSection( &_csec_seq);
	return id;
}

int	CcmppSocket::_send( char *buf, int len)
{
	int err;

	EnterCriticalSection( &_csec_snd);
	err = send( _soc, buf, len, 0);
	LeaveCriticalSection( &_csec_snd);

	return err;
}

int	CcmppSocket::_recv( char *buf, int len)
{
	int err;

	EnterCriticalSection( &_csec_recv);
	err = recv( _soc, buf, len, 0);
#ifdef _DEBUG_EX
	//sprintf(_revp," Recieve Data=%s Needed Length=%d.",buf, len);
	//_dbgeventlst.push_back(_revp);

	if (err!=SOCKET_ERROR)
	{
		memcpy( (void *)_revp, (void *)buf, err);
		_revp+=err;
		//memset(_revp,0xFF,16);
		//_revp+=16;
	}
#endif
	LeaveCriticalSection( &_csec_recv);

	return err;
}