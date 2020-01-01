#include "stdafx.h"
#include "ServerWins.h"
#include <iostream>
#include <string>
#include <thread>


#pragma comment (lib,"ws2_32")

CServerWins::CServerWins(const char* ip, int port) :m_terminal(false)
{
	//必须进行如下初始化， 否则socket()会返回10093错误
	//初始化WSA  
	WORD sockVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (WSAStartup(sockVersion, &wsaData) != 0) //通过一个进程初始化ws2_32.dll
	{
		std::cout << "Initialize WSA failed" << std::endl;
		return;
	}

	//创建套接字
	m_slisten = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_slisten == INVALID_SOCKET)
	{
		printf("%d\n", WSAGetLastError());
		printf("%s\n", "Socket Error!");

		return;
	}
	struct sockaddr_in sin;
	std::string bindip;
	sin.sin_family = AF_INET; //设置地址家族
	sin.sin_port = htons(port); //设置端口号，inet_addr("192.168.1.0");
	if (0 == ip[0])
	{
		sin.sin_addr.S_un.S_addr = INADDR_ANY; //设置地址 INADDR_ANY:转换过来就是0.0.0.0，泛指本机的意思，也就是表示本机的所有IP，因为有些机子不止一块网卡，多网卡的情况下，这个就表示所有网卡ip地址的意思。
		char szHost[256];
		::gethostname(szHost, 256);

		hostent *pHost = ::gethostbyname(szHost);

		in_addr addr;
		for (int i = 0;; i++)
		{
			char *p = pHost->h_addr_list[i];
			if (p == NULL)
				break;
			memcpy(&addr.S_un.S_addr, p, pHost->h_length);
			char *slzp = ::inet_ntoa(addr);
			bindip = slzp;
		}
	}
	else
	{
		sin.sin_addr.S_un.S_addr = inet_addr(ip);
		bindip = ip;
	}

	if (SOCKET_ERROR == bind(m_slisten, (LPSOCKADDR)&sin, sizeof(sin)))
	{
		printf("Bind Error!%d", WSAGetLastError());
	}
	if (listen(m_slisten, 20) == SOCKET_ERROR)  //套接字, 为该套接字排队的最大连接数
												//此时， slisten 变为监听套接字
	{
		printf("%s\n", "Listen Error!");
		closesocket(m_slisten); //关闭监听套接字
		WSACleanup(); //终止Ws2_32.dll的使用
		return;
	}

	std::cout << "bind " << bindip << ":" << port << "\n";
}

void CServerWins::keepClient(Client* cClient)
{
	char revData[1025]; //接收回来的数据
	int ret = 0; //接收回来的字节数

	while (1)
	{
		ret = recv(cClient->sock, revData, 1024, 0);
		if (ret > 0)
		{
			revData[ret] = '\0';
			std::cout << "Client " << inet_ntoa(cClient->sin_addr)<< "(" << cClient->sock << ")" << " say : " << revData << std::endl;
			//发送数据  
			send(cClient->sock, "Server get msg!", strlen("Server get msg!"), 0);
		}
		else
		{
			closesocket(cClient->sock);
			std::cout << "Client" << inet_ntoa(cClient->sin_addr) << "(" << cClient->sock << ")" << " session end " << std::endl;
			m_io_mutex.lock();
			for (auto itr = m_clients.begin(); itr != m_clients.end(); ++itr)
			{
				if ((*itr)->sock == cClient->sock)
				{
					closesocket(cClient->sock);
					delete *itr;
					m_clients.erase(itr);
					break;
				}
			}
			m_io_mutex.unlock();
			break;
		}
	}

	//连接套接字， 要发送数据所存储位置对应的地址， 长度
	//closesocket(sClient); //关闭连接套接字
	//std::cout << "session end" << std::endl;
	//Sleep(30);
}


void CServerWins::RecMsg()
{
	std::cout << "Rec thread begin ..." << std::endl;
	struct sockaddr_in remoteAddr;     //存储 通过accept 得到的 客户端IP地址
	int nAddrlen = sizeof(remoteAddr); //IP地址长度
	while (!m_terminal)
	{
		std::cout << "thread is running, current have " << m_clients.size() << " client count" << std::endl;

		SOCKET cClient = accept(m_slisten, (SOCKADDR*)&remoteAddr, &nAddrlen);
		if (cClient == INVALID_SOCKET)
		{
			printf("accept error !");
			continue;
		}
		std::cout << "Client" << inet_ntoa(remoteAddr.sin_addr) << "(" << cClient << ")" << " session start" << std::endl;

		Client* c = new Client(cClient, remoteAddr.sin_addr, remoteAddr.sin_port);
		m_io_mutex.lock();
		m_clients.push_back(c);
		m_io_mutex.unlock();
		std::thread t(&CServerWins::keepClient, this, c);
		t.detach();
	}

	std::cout << "Rec thread end..." << std::endl;
}
void CServerWins::stopRec()
{
	m_terminal = true;
}
CServerWins::~CServerWins()
{
	std::cout << "~CServerWins()" << std::endl;
	closesocket(m_slisten); //关闭监听套接字
	WSACleanup(); //终止Ws2_32.dll的使用
}
//————————————————
//版权声明：本文为CSDN博主「csdn_WHB」的原创文章，遵循 CC 4.0 BY - SA 版权协议，转载请附上原文出处链接及本声明。
//原文链接：https ://blog.csdn.net/CSDN_WHB/article/details/89160002