#pragma once

#include "stdafx.h"
#include <winsock2.h>
#include <queue>
#include "cmppdefines.h"
#include "sgip.h"

const int nCMPP_RCV_WINDOW_SIZE  =2048 ;          //  ���մ��ڴ�С

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
	void	_logout ();					//	ע��
	void	_exit	();					//	��������̣߳��˳��ӿ�
	static	DWORD	WINAPI	thread_send( LPVOID pdata);
	static	DWORD	WINAPI	thread_recv( LPVOID pdata);
	static	DWORD	WINAPI	thread_actv( LPVOID pdata);
private:
	char*	_timestamp( char *buf);		//	����10λ��ʱ���
	__int64	_ntoh64	( __int64 inval);	//	��64λ������ת�������ֽ�˳��
	__int64	_hton64	( __int64 inval);	//	��64λ������ת�������ֽ�˳��
	int		_getseqid();				//	�õ���һ�����ݰ�����ˮ��
	int		_send( char *buf, int len); //	ͬ����������
	int		_recv( char *buf, int len); //	ͬ����������

private:
	SOCKET	_soc;					//	���ӷ��������׽���
	bool	_binitialized;			//	�ӿڳ�ʼ����־
	bool	_bexitting;				//	�����߳��˳�
	char	_spid[10];				//	6 λ��ҵ����
	char	_ismg[20];				//	��������IP��ַ
	char	_passwd[20];			//	��ҵ��½�������صĿ���
	unsigned short _port;			//	�������صĶ˿ڣ�Ĭ��7890

	int		_seqid;					//	��ˮ�ţ���ͬ��
	HANDLE	_hsend;					//	�����߳�
	HANDLE	_hrecv;					//	�����߳�
	HANDLE	_hactv;					//	��·ά���߳�
	HANDLE	_hguard;				//	�����̣߳������Զ�����
	HANDLE	_hsema_wnd;				//	�������ݴ��ڵĿո�
	HANDLE	_hevnt_data;			//	�������ύ�����ݱ�
    HANDLE  _hsema_rev;             //  ���ݽ��մ��ڿ����С

//	HANDLE  _hdeliver;				// �����¼�֪ͨ

	CRITICAL_SECTION	_csec_wnd;	//	ͬ�����ݴ���
	CRITICAL_SECTION	_csec_snd;	//	ͬ����������
	CRITICAL_SECTION	_csec_recv;	//	ͬ����������
	CRITICAL_SECTION	_csec_seq;	//	ͬ����ˮ��
    CRITICAL_SECTION    _csec_revwnd; //  ͬ�����ݽ��մ���

	CMPP_PACKAGE	_window[nCMPP_WINDOW_SIZE];	//	���ݴ��ڣ���ͬ��
};