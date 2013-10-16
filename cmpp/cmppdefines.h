#ifndef _CMPP_H_
#define _CMPP_H_

const int nCMPP_VERSION          =0x30;			//	CMPP3.0
const int nCMPP_WINDOW_SIZE		 =16;			//	CMPP�������ڵĴ�С
const int nCMPP_PKG_SIZE		 =400;			//	CMPP���ݰ�����󳤶�

const int nCMPP_CONNECT          =0x00000001;	//	��������
const int nCMPP_CONNECT_RESP     =0x80000001;	//	��������Ӧ��
const int nCMPP_TERMINATE        =0x00000002;	//	��ֹ����
const int nCMPP_TERMINATE_RESP   =0x80000002;	//	��ֹ����Ӧ��
const int nCMPP_SUBMIT           =0x00000004;	//	�ύ����
const int nCMPP_SUBMIT_RESP      =0x80000004;	//	�ύ����Ӧ��
const int nCMPP_DELIVER          =0x00000005;	//	�����·�
const int nCMPP_DELIVER_RESP     =0x80000005;	//	�·�����Ӧ��
const int nCMPP_QUERY            =0x00000006;	//	����״̬��ѯ
const int nCMPP_QUERY_RESP       =0x80000006;	//	����״̬��ѯӦ��
const int nCMPP_CANCEL           =0x00000007;	//	ɾ������
const int nCMPP_CANCEL_RESP      =0x80000007;	//	ɾ������Ӧ��
const int nCMPP_ACTIVE_TEST      =0x00000008;	//	�������
const int nCMPP_ACTIVE_TEST_RESP =0x80000008;	//	�������Ӧ��

const int eCMPP_INIT_OK			 =0;			//	�ӿڳ�ʼ���ɹ�
const int eCMPP_INIT_CONNECT	 =-1;			//	�������ӷ�����
const int eCMPP_INIT_INVAL_ADDR  =2;			//	�Ƿ�Դ��ַ
const int eCMPP_INIT_AUTH		 =3;			//	��֤��
const int eCMPP_INIT_VERSION	 =4;			//	�汾̫��
const int eCMPP_INIT_UNKNOWN	 =5;			//	��������

const int eCMPP_NEED_INIT		 =6;			//	�ӿ���δ��ʼ��

#pragma pack(1)

typedef struct _CMPP_HEAD
{
	int size;									//	���ݱ��ĳ���
	int	cmdid;									//	������
	int	seqid;									//	��ˮ��

} CMPP_HEAD;

typedef	struct _CMPP_PACKAGE
{
	CMPP_HEAD	head;							//	���ݱ��İ�ͷ
	char		data[nCMPP_PKG_SIZE];			//	Ҫ���͵�����
	int			n;								//	�ط���������ʼֵ3
	time_t		t;								//	�´η��͵�ʱ��
} CMPP_PACKAGE;

typedef struct _CMPP_CONNECT
{
	unsigned char spid[6];						//	9��ͷ��6λ��ҵ��
	unsigned char digest[16];					//	MD5����ǩ��
	unsigned char ver;							//	�汾
	int timestamp;
} CMPP_CONNECT;

typedef struct _CMPP_CONNECT_RESP
{
	__int32 status;					        	//	״̬��ʶ
	unsigned char digest[16];					//	��������MD5����ǩ��
	unsigned char ver;							//	�������İ汾
} CMPP_CONNECT_RESP;

/******************************************************************************
  *	ע�⣺�����Ŀ�ĵ��ֻ�����ֻ����дһ��������desttotal�����1
  *       ����Ķ���Ϣ����Ϊ�̶����ȣ����͵�ʱ����Ҫת���ɱ䳤
******************************************************************************/
typedef struct _CMPP_SUBMIT
{
	__int64 msgid;								//	��Ϣ��ʶ
	unsigned char pkgtotal;						//	��ͬMsg_Id����Ϣ������
	unsigned char pkgnumber;					//	��ͬMsg_Id����Ϣ���
	unsigned char delivery;						//	�Ƿ�Ҫ�󷵻�״̬ȷ�ϱ���
	unsigned char msglevel;						//	��Ϣ����
	unsigned char serviceid[10];				//	ҵ������
	unsigned char feeusertype;					//	�Ʒ��û������ֶ�
	unsigned char feenumber[32];				//	���Ʒ��û��ĺ���
	unsigned char srcttype;						//  ���Ʒ��û��ĺ������ͣ�0����ʵ���룻1��α�롣
	unsigned char tppid;						//	TP-Protocol-Identifier
	unsigned char tpudhi;						//	TP-User-Data-Header-Indicator
	unsigned char msgfmt;						//	��Ϣ��ʽ��8��UCS2����
	unsigned char msgsrc[6];					//	��Ϣ������Դ(SP_Id)
	unsigned char feetype[2];					//	�ʷ����
	unsigned char feecode[6];					//	�ʷѴ��루�Է�Ϊ��λ��
	unsigned char validtime[17];				//	�����Ч�ڣ��μ�SMPP3.3
	unsigned char attime[17];					//	��ʱ����ʱ�䣬�μ�SMPP3.3 
	unsigned char srcnumber[21];				//	Դ����
	unsigned char desttotal;					//	������Ϣ���û�����(===1)
	unsigned char destnumbers[32];				//	���ն��ŵ�MSISDN����
	unsigned char desttype;						//  ���ն��ŵ��û��ĺ������ͣ�0����ʵ���룻1��α�롣
	unsigned char msglen;						//	��Ϣ����(<=140)
	unsigned char msgcontent[160];				//	��Ϣ����
	unsigned char reserve[20];					//	����
} CMPP_SUBMIT;

typedef struct _CMPP_SUBMIT_RESP
{
	__int64 msgid;								//	��Ϣ��ʶ
	__int32 result;	        					//	���
} CMPP_SUBMIT_RESP;

typedef struct _CMPP_SUBMIT_RESP_EX
{
	int	seqid;
	__int64 msgid;								//	��Ϣ��ʶ
	__int32 result;	        					//	���
} CMPP_SUBMIT_RESP_EX;

typedef struct _CMPP_QUERY
{
	unsigned char date[8];						//	����YYYYMMDD(��ȷ����)
	unsigned char type;							//	��ѯ���0/1
	unsigned char serviceid[10];				//	��ѯ��(Service_Id)
	unsigned char reserve[8];					//	����
} CMPP_QUERY;

typedef struct _CMPP_QUERY_RESP
{
	unsigned char date[8];						//	����YYYYMMDD(��ȷ����)
	unsigned char type;							//	��ѯ���0/1
	unsigned char serviceid[10];				//	��ѯ��(Service_Id)
	int	mtmsgtotal;								//	��SP������Ϣ����
	int	mtusertotal;							//	��SP�����û�����
	int	mtsuccess;								//	�ɹ�ת������
	int	mtwait;									//	��ת������
	int	mtfail;									//	ת��ʧ������
	int	mosuccess;								//	��SP�ɹ��ʹ�����
	int	mowait;									//	��SP���ʹ�����
	int	mofail;									//	��SP�ʹ�ʧ������
} CMPP_QUERY_RESP;

typedef struct _CMPP_DELIVER
{
	__int64 msgid;								//	��Ϣ��ʶ
	unsigned char destnumber[21];				//	Ŀ�ĺ���
	unsigned char serviceid[10];				//	ҵ������
	unsigned char tppid;						//	TP-Protocol-Identifier
	unsigned char tpudhi;						//	TP-User-Data-Header-Indicator
	unsigned char msgfmt;						//	��Ϣ��ʽ��8��UCS2����
	unsigned char srcnumber[32];				//	Դ�ն�MSISDN����
    unsigned char srctype;                      //  Դ�ն�����
	unsigned char delivery;						//	�Ƿ�Ϊ״̬����
	unsigned char msglen;						//	��Ϣ����
	unsigned char msgcontent[200];				//	��Ϣ����
    unsigned char LinkID[20] ;                   //  LinkID
} CMPP_DELIVER;

typedef struct _CMPP_DELIVER_STATUS
{
    unsigned char LinkID[20] ;                   //  LinkID
	__int64 msgid;								//	��Ϣ��ʶ
	unsigned char stat[7];						//	���Ͷ��ŵ�Ӧ����
	unsigned char submittime[10];				//	
	unsigned char donetime[10];					//	
	unsigned char destterminalid[32];			//	����CMPP_SUBMIT��Ϣ��Ŀ���ն�
	int	smscsequence;							//  MSC����״̬�������Ϣ���е���Ϣ��ʶ

} CMPP_DELIVER_STATUS;

typedef struct _CMPP_DELIVER_RESP
{
	__int64 msgid;								//	��Ϣ��ʶ
	unsigned char result;						//	���
} CMPP_DELIVER_RESP;

typedef struct _CMPP_CANCEL
{
	__int64 msgid;								//	��Ϣ��ʶ
} CMPP_CANCEL;

typedef struct _CMPP_CANCEL_RESP
{
	unsigned char result;						//	�ɹ���ʶ(0/1)
} CMPP_CANCEL_RESP;

typedef struct _CMPP_ACTIVE_TEST_RESP
{
	unsigned char reserve;						//	����
} CMPP_ACTIVE_TEST_RESP;

typedef struct _CMPP_FUNC_MAP
{
	int		cmdid;								//	������
	void	(*func)( CMPP_PACKAGE &pkg);		//	�������
} CMPP_FUNC_MAP;

#pragma pack()

#endif

