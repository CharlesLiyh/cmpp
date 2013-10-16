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

//�����յ�����dump �� _revdebug[]

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

	//	��ʼ��˽�б���
	_binitialized = false;
	_bexitting = false;
	_seqid	= 0;
	memset( (void *)_window, 0, sizeof( _window));
	InitializeCriticalSection( &_csec_wnd);
	InitializeCriticalSection( &_csec_snd);
	InitializeCriticalSection( &_csec_recv);
	InitializeCriticalSection( &_csec_seq);
	_hsema_wnd = CreateSemaphore(			//	���������ź���
		NULL,
		nCMPP_WINDOW_SIZE,
		nCMPP_WINDOW_SIZE,
		NULL);
	_hevnt_data = CreateEvent(				//	������ʾ���͵��¼�
		NULL,
		false,
		false,
		NULL);
	/*	_hdeliver = CreateEvent(				//	���������¼�
	NULL,
	false,
	false,
	NULL);*/
	//	��ʼ������
	WSADATA wsaData;
	WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
#ifdef _DEBUG_EX
	_dbgeventlst.push_back("End   Class CcmppSocket Constructor.");
#endif
}

CcmppSocket::~CcmppSocket()
{
	//	����CMPP_TERMINATE���ӷ�����ע��
#ifdef _DEBUG_EX
	_dbgeventlst.push_back("Begin Class CcmppSocket Desstructor.");
#endif
	if( _binitialized)
		_exit();
	WSACleanup();
	//	����ٽ�����
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
spid		��ҵ����		���磺901001
passwd	��½����		���磺test
ismg		�������صĵ�ַ�����磺127.0.0.1
port		�������صĶ˿ڣ����磺7890
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
	//	�������ã��Ա�����
	strcpy( _spid,	spid);
	strcpy( _passwd,passwd);
	strcpy( _ismg,	ismg);
	_port = port;
	//	�������͡��������ݵ��߳�
#ifdef _DEBUG_EX
	_dbgeventlst.push_back("Begin Thread Send SMS.");
#endif
	_hsend = CreateThread(	//	���ŷ����߳�
		NULL,
		NULL,
		thread_send,
		(LPVOID)this,
		0,
		NULL);
#ifdef _DEBUG_EX
	_dbgeventlst.push_back("Begin Thread Recieve SMS.");
#endif
	_hrecv = CreateThread(	//	���Ž����߳�
		NULL,
		NULL,
		thread_recv,
		(LPVOID)this,
		0,
		NULL);
#ifdef _DEBUG_EX
	_dbgeventlst.push_back("Begin Thread Activate.");
#endif
	_hactv = CreateThread(	//	��·���
		NULL,
		NULL,
		thread_actv,
		(LPVOID)this,
		0,
		NULL);
	//	��ʼ���ɹ������óɹ���־
	_binitialized = true;
	//	������ǰʱ��Ƭ
	Sleep( 0);

#ifdef _DEBUG_EX
	_dbgeventlst.push_back("Connection Initializtion Success.");
#endif
	return eCMPP_INIT_OK;
}

/******************************************************************************
msg				��������ύ������
dwMilliseconds	�ڳɹ��Ľ����ݲ������ݴ���ǰ�ȴ���ʱ��

����ֵ			����
===============	==============================		
0					�ɹ�
eCMPP_NEED_INIT	�ӿ�δ��ʼ��
WAIT_TIMEOUT		������ʱ
WAIT_ABANDONED	�����߳��쳣�˳����������������
******************************************************************************/
int	CcmppSocket::Submit( CMPP_SUBMIT &msg, int &iseqid, DWORD dwMilliseconds)
{
	CMPP_PACKAGE	pkg;
	CMPP_HEAD		&head = (CMPP_HEAD		&)pkg.head;
	int	err, nsize;

	if(!_binitialized)
		return eCMPP_NEED_INIT;

	pkg.n = 0;							//	����ʧ�ܣ����ط�����
	pkg.t = time( NULL);				//	��������

	nsize = sizeof( CMPP_HEAD) + sizeof( CMPP_SUBMIT);
	//	��ΪCMPPЭ���а��ĳ����ǿɱ�ģ����Ҷ�������ݽṹ��
	//	���ĳ��Ȳ��õ�����󳤶ȣ�����������Ҫ����
	nsize = nsize + msg.msglen - sizeof( msg.msgcontent);
	head.size = htonl( nsize);
	head.cmdid= htonl( nCMPP_SUBMIT);
	head.seqid= htonl( _getseqid());
	iseqid =  ntohl (head.seqid); //����seqid��״̬���ش���

	memcpy( (void *)pkg.data, (void *)&msg, sizeof( msg));
	//	�����8���ֽڵı������ݿ������ʵ���λ��
	memcpy(
		(void *)(pkg.data + nsize - sizeof( msg.reserve) - sizeof( CMPP_HEAD)),
		(void *)msg.reserve,
		sizeof( msg.reserve));
	//	�Ⱥ����ݷ��ʹ��ڵĿ�λ
	err = WaitForSingleObject( _hsema_wnd, dwMilliseconds);
	//	�ȴ���ʱ������쳣
	if( err != WAIT_OBJECT_0)
		return err;
	//	�������ݷ��ʹ��ڵ�ʹ��Ȩ
	EnterCriticalSection( &_csec_wnd);

	int i;
	for(i=0; i<nCMPP_WINDOW_SIZE; i++)
	{
		//	�ҵ�һ����λ
		if( _window[i].head.cmdid == 0)
			break;
	}
	memcpy( (void *)&_window[i], (void *)&pkg, sizeof( pkg));
	//	_window[i].n = 0;

	LeaveCriticalSection( &_csec_wnd);
	//	�������ݷ����߳�
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
	//	���㵥��HASH������ֵ
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
	//	���������֤����
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
	//	���շ��ص����ݰ�
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
	//	�������߳��˳�
	_bexitting = true;

	Sleep( 1000);
	//	�ر�����ǿ�ƽ���������δ�˳����߳�
	_bexitting = false;

	nthreads = sizeof( threads) / sizeof( HANDLE);
	for( i=0; i<nthreads; i++)
	{
		TerminateThread( threads[i], 0);
		CloseHandle( threads[i]);
	}
	//	ע��
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
���ݷ����߳�

�������ύ�����ݱ�����������
����60��δ�յ���Ӧ�����ط�
******************************************************************************/
DWORD	WINAPI	CcmppSocket::thread_send( LPVOID pdata)
{
	CcmppSocket		&cmpp = *( CcmppSocket *)pdata;
	CMPP_PACKAGE	window[nCMPP_WINDOW_SIZE];
	int	i;
	int	err;
	int	nsize;
	int	dwMilliseconds = 1000;		//	��ѯ���Ϊ1000����
	for( ;;)
	{
		//	��ѯ���1��
		err = WaitForSingleObject(
			cmpp._hevnt_data,
			dwMilliseconds);
		//	�����ˣ������߳�
		if( err == WAIT_FAILED)
			break;
		//	�������ݷ��ʹ��ڵ�ʹ��Ȩ
		EnterCriticalSection( &cmpp._csec_wnd);
		//	�������ݵ��Լ����ڴ����򣬱������ٽ����з�������
		memcpy( (void *)window, (void *)cmpp._window, sizeof( window));
		//	��Ѱ���޸ĵ���ʱ��
		for( i=0; i<nCMPP_WINDOW_SIZE; i++)
		{
			//	��λ������
			if( cmpp._window[i].head.cmdid == 0)
				continue;
			//	δ������ʱ�䣬����
			if( cmpp._window[i].t > time( NULL))
				continue;
			//	������һ�η��͵�ʱ�䣬��ǰʱ�䣫60��
			cmpp._window[i].t += 60;

			//  ������ʹ�������3�Σ��Ӷ��������
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
		//	��Ѱ�����͵��ڵ�����
		for( i=0; i<nCMPP_WINDOW_SIZE; i++)
		{
			//	��λ������
			if( window[i].head.cmdid == 0)
				continue;
			//	δ������ʱ�䣬����
			if( window[i].t > time( NULL))
				continue;

			if( window[i].n >= 3 )
				continue;

			//	����
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
			//	���ͳ��������߳�
			if( err != nsize)
				return 0;
		}
		//	���߳������˳�
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
	//	��ѯ���1��
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
		//	����
		if( err == SOCKET_ERROR)
		{
#ifdef _DEBUG_EX
			sprintf(_dbgtemp,"Thread Recieve : Socket Wait for Data Error.Error Code=%d",WSAGetLastError());
			_dbgeventlst.push_back(_dbgtemp);
#endif
			continue;//break;
		}
		//	��ʱ
		if( err == 0)
			continue;
		//	�Ƚ��հ�ͷ���֣���ȷ�����Ĵ�С������
		err = cmpp._recv( (char *)&pkg.head, sizeof( pkg.head));
		if( err != sizeof( pkg.head))
		{
#ifdef _DEBUG_EX
			sprintf(_dbgtemp,"Thread Recieve : Recieve Header Return Code=%d Needed Length=%d Error=%d.",err,sizeof( pkg.head),WSAGetLastError());
			_dbgeventlst.push_back(_dbgtemp);
#endif
			continue;//break;
		}
		//	���հ���
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
				//	�������ݷ��ʹ��ڵ�ʹ��Ȩ
				EnterCriticalSection( &cmpp._csec_wnd);
				//	ɾ����Ӧ��ˮ�ŵİ�
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
				//	�ͷ�һ�����ڸ���
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
		//	���߳������˳�
		if( cmpp._bexitting)
			break;
	}
#ifdef _DEBUG_EX
	_dbgeventlst.push_back("Thread Recieve : Thread End.");
#endif

	return 0;

}

/******************************************************************************
*    ���ݽ����̣߳��Ӷ˿������ݴ����������
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
*	Ϊ�˱�֤�̼߳�ʱ�����˳����趨��ѯ���Ϊ1��
*	CMPP3.0�������·�����Ϊ180��
*	Ϊ�˼��ݸ������ҵ�ʵ�֣���Ϊ30��
******************************************************************************/
DWORD	WINAPI	CcmppSocket::thread_actv( LPVOID pdata)
{
	CcmppSocket &cmpp = *( CcmppSocket *)pdata;
	CMPP_HEAD	msg;
	int			c = 0,		//	����
		n = 30;		//	�ȴ�������
	for( ;;)
	{
		Sleep( 1000);
		//	���߳������˳�
		if( cmpp._bexitting)
			break;
		if( ++ c < n)
			continue;
		c = 0;
		//	������ݷ���Ȩ����������·���԰�
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