//Description             : sgipЭ����صĳ����Ķ���


#ifndef _SGIP_H_
#define _SGIP_H_

const int nSGIP_VERSION            =0x18;           // SGIP1.2

const int nSGIP_BIND               =0x00000001;		// ��������
const int nSGIP_BIND_RESP          =0x80000001;		// ���������Ӧ��
const int nSGIP_UNBIND             =0x00000002;		// ��ֹ����
const int nSGIP_UNBIND_RESP        =0x80000002;		// ��ֹ���ӵ�Ӧ��
const int nSGIP_SUBMIT             =0x00000003;		// �ύ����
const int nSGIP_SUBMIT_RESP        =0x80000003;		// �ύ����Ӧ��
const int nSGIP_DELIVER            =0x00000004;		// �����·�
const int nSGIP_DELIVER_RESP       =0x80000004;		// �·�����Ӧ��
const int nSGIP_REPORT             =0x00000005;		// ����״̬����
const int nSGIP_REPORT_RESP        =0x80000005;		// ����״̬�����Ӧ��
const int nSGIP_USERRPT            =0x00000011;		// ������
const int nSGIP_USERRPT_RESP       =0x80000011;		// ������
const int nSGIP_TRACE              =0x00001000;		// �����ѯMT��Ϣ״̬
const int nSGIP_TRACE_RESP         =0x80001000;		// ״̬Ӧ��

#endif