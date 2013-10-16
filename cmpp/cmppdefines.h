#ifndef _CMPP_H_
#define _CMPP_H_

const int nCMPP_VERSION          =0x30;			//	CMPP3.0
const int nCMPP_WINDOW_SIZE		 =16;			//	CMPP滑动窗口的大小
const int nCMPP_PKG_SIZE		 =400;			//	CMPP数据包的最大长度

const int nCMPP_CONNECT          =0x00000001;	//	请求连接
const int nCMPP_CONNECT_RESP     =0x80000001;	//	请求连接应答
const int nCMPP_TERMINATE        =0x00000002;	//	终止连接
const int nCMPP_TERMINATE_RESP   =0x80000002;	//	终止连接应答
const int nCMPP_SUBMIT           =0x00000004;	//	提交短信
const int nCMPP_SUBMIT_RESP      =0x80000004;	//	提交短信应答
const int nCMPP_DELIVER          =0x00000005;	//	短信下发
const int nCMPP_DELIVER_RESP     =0x80000005;	//	下发短信应答
const int nCMPP_QUERY            =0x00000006;	//	短信状态查询
const int nCMPP_QUERY_RESP       =0x80000006;	//	短信状态查询应答
const int nCMPP_CANCEL           =0x00000007;	//	删除短信
const int nCMPP_CANCEL_RESP      =0x80000007;	//	删除短信应答
const int nCMPP_ACTIVE_TEST      =0x00000008;	//	激活测试
const int nCMPP_ACTIVE_TEST_RESP =0x80000008;	//	激活测试应答

const int eCMPP_INIT_OK			 =0;			//	接口初始化成功
const int eCMPP_INIT_CONNECT	 =-1;			//	不能连接服务器
const int eCMPP_INIT_INVAL_ADDR  =2;			//	非法源地址
const int eCMPP_INIT_AUTH		 =3;			//	认证错
const int eCMPP_INIT_VERSION	 =4;			//	版本太高
const int eCMPP_INIT_UNKNOWN	 =5;			//	其他错误

const int eCMPP_NEED_INIT		 =6;			//	接口尚未初始化

#pragma pack(1)

typedef struct _CMPP_HEAD
{
	int size;									//	数据报的长度
	int	cmdid;									//	命令码
	int	seqid;									//	流水号

} CMPP_HEAD;

typedef	struct _CMPP_PACKAGE
{
	CMPP_HEAD	head;							//	数据报的包头
	char		data[nCMPP_PKG_SIZE];			//	要发送的数据
	int			n;								//	重发次数，初始值3
	time_t		t;								//	下次发送的时间
} CMPP_PACKAGE;

typedef struct _CMPP_CONNECT
{
	unsigned char spid[6];						//	9开头的6位企业码
	unsigned char digest[16];					//	MD5数字签名
	unsigned char ver;							//	版本
	int timestamp;
} CMPP_CONNECT;

typedef struct _CMPP_CONNECT_RESP
{
	__int32 status;					        	//	状态标识
	unsigned char digest[16];					//	服务器的MD5数字签名
	unsigned char ver;							//	服务器的版本
} CMPP_CONNECT_RESP;

/******************************************************************************
  *	注意：这里的目的地手机号码只能填写一个，所以desttotal恒等于1
  *       这里的短消息内容为固定长度，发送的时候需要转换成变长
******************************************************************************/
typedef struct _CMPP_SUBMIT
{
	__int64 msgid;								//	信息标识
	unsigned char pkgtotal;						//	相同Msg_Id的信息总条数
	unsigned char pkgnumber;					//	相同Msg_Id的信息序号
	unsigned char delivery;						//	是否要求返回状态确认报告
	unsigned char msglevel;						//	信息级别
	unsigned char serviceid[10];				//	业务类型
	unsigned char feeusertype;					//	计费用户类型字段
	unsigned char feenumber[32];				//	被计费用户的号码
	unsigned char srcttype;						//  被计费用户的号码类型，0：真实号码；1：伪码。
	unsigned char tppid;						//	TP-Protocol-Identifier
	unsigned char tpudhi;						//	TP-User-Data-Header-Indicator
	unsigned char msgfmt;						//	信息格式，8：UCS2编码
	unsigned char msgsrc[6];					//	信息内容来源(SP_Id)
	unsigned char feetype[2];					//	资费类别
	unsigned char feecode[6];					//	资费代码（以分为单位）
	unsigned char validtime[17];				//	存活有效期，参见SMPP3.3
	unsigned char attime[17];					//	定时发送时间，参见SMPP3.3 
	unsigned char srcnumber[21];				//	源号码
	unsigned char desttotal;					//	接收信息的用户数量(===1)
	unsigned char destnumbers[32];				//	接收短信的MSISDN号码
	unsigned char desttype;						//  接收短信的用户的号码类型，0：真实号码；1：伪码。
	unsigned char msglen;						//	信息长度(<=140)
	unsigned char msgcontent[160];				//	信息内容
	unsigned char reserve[20];					//	保留
} CMPP_SUBMIT;

typedef struct _CMPP_SUBMIT_RESP
{
	__int64 msgid;								//	信息标识
	__int32 result;	        					//	结果
} CMPP_SUBMIT_RESP;

typedef struct _CMPP_SUBMIT_RESP_EX
{
	int	seqid;
	__int64 msgid;								//	信息标识
	__int32 result;	        					//	结果
} CMPP_SUBMIT_RESP_EX;

typedef struct _CMPP_QUERY
{
	unsigned char date[8];						//	日期YYYYMMDD(精确至日)
	unsigned char type;							//	查询类别0/1
	unsigned char serviceid[10];				//	查询码(Service_Id)
	unsigned char reserve[8];					//	保留
} CMPP_QUERY;

typedef struct _CMPP_QUERY_RESP
{
	unsigned char date[8];						//	日期YYYYMMDD(精确至日)
	unsigned char type;							//	查询类别0/1
	unsigned char serviceid[10];				//	查询码(Service_Id)
	int	mtmsgtotal;								//	从SP接收信息总数
	int	mtusertotal;							//	从SP接收用户总数
	int	mtsuccess;								//	成功转发数量
	int	mtwait;									//	待转发数量
	int	mtfail;									//	转发失败数量
	int	mosuccess;								//	向SP成功送达数量
	int	mowait;									//	向SP待送达数量
	int	mofail;									//	向SP送达失败数量
} CMPP_QUERY_RESP;

typedef struct _CMPP_DELIVER
{
	__int64 msgid;								//	信息标识
	unsigned char destnumber[21];				//	目的号码
	unsigned char serviceid[10];				//	业务类型
	unsigned char tppid;						//	TP-Protocol-Identifier
	unsigned char tpudhi;						//	TP-User-Data-Header-Indicator
	unsigned char msgfmt;						//	信息格式，8：UCS2编码
	unsigned char srcnumber[32];				//	源终端MSISDN号码
    unsigned char srctype;                      //  源终端类型
	unsigned char delivery;						//	是否为状态报告
	unsigned char msglen;						//	消息长度
	unsigned char msgcontent[200];				//	消息内容
    unsigned char LinkID[20] ;                   //  LinkID
} CMPP_DELIVER;

typedef struct _CMPP_DELIVER_STATUS
{
    unsigned char LinkID[20] ;                   //  LinkID
	__int64 msgid;								//	信息标识
	unsigned char stat[7];						//	发送短信的应答结果
	unsigned char submittime[10];				//	
	unsigned char donetime[10];					//	
	unsigned char destterminalid[32];			//	发送CMPP_SUBMIT消息的目标终端
	int	smscsequence;							//  MSC发送状态报告的消息体中的消息标识

} CMPP_DELIVER_STATUS;

typedef struct _CMPP_DELIVER_RESP
{
	__int64 msgid;								//	信息标识
	unsigned char result;						//	结果
} CMPP_DELIVER_RESP;

typedef struct _CMPP_CANCEL
{
	__int64 msgid;								//	信息标识
} CMPP_CANCEL;

typedef struct _CMPP_CANCEL_RESP
{
	unsigned char result;						//	成功标识(0/1)
} CMPP_CANCEL_RESP;

typedef struct _CMPP_ACTIVE_TEST_RESP
{
	unsigned char reserve;						//	保留
} CMPP_ACTIVE_TEST_RESP;

typedef struct _CMPP_FUNC_MAP
{
	int		cmdid;								//	命令字
	void	(*func)( CMPP_PACKAGE &pkg);		//	函数入口
} CMPP_FUNC_MAP;

#pragma pack()

#endif

