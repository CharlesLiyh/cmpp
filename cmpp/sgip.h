//Description             : sgip协议相关的常量的定义


#ifndef _SGIP_H_
#define _SGIP_H_

const int nSGIP_VERSION            =0x18;           // SGIP1.2

const int nSGIP_BIND               =0x00000001;		// 连接请求
const int nSGIP_BIND_RESP          =0x80000001;		// 连接请求的应答
const int nSGIP_UNBIND             =0x00000002;		// 终止连接
const int nSGIP_UNBIND_RESP        =0x80000002;		// 终止连接的应答
const int nSGIP_SUBMIT             =0x00000003;		// 提交短信
const int nSGIP_SUBMIT_RESP        =0x80000003;		// 提交短信应答
const int nSGIP_DELIVER            =0x00000004;		// 短信下发
const int nSGIP_DELIVER_RESP       =0x80000004;		// 下发短信应答
const int nSGIP_REPORT             =0x00000005;		// 短信状态报告
const int nSGIP_REPORT_RESP        =0x80000005;		// 短信状态报告的应答
const int nSGIP_USERRPT            =0x00000011;		// ？？？
const int nSGIP_USERRPT_RESP       =0x80000011;		// ？？？
const int nSGIP_TRACE              =0x00001000;		// 请求查询MT消息状态
const int nSGIP_TRACE_RESP         =0x80001000;		// 状态应答

#endif