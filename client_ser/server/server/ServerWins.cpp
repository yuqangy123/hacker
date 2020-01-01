#include "stdafx.h"
#include "ServerWins.h"
#include <iostream>
#include <string>
#include <thread>


#pragma comment (lib,"ws2_32")

CServerWins::CServerWins(const char* ip, int port) :m_terminal(false)
{
	//����������³�ʼ���� ����socket()�᷵��10093����
	//��ʼ��WSA  
	WORD sockVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (WSAStartup(sockVersion, &wsaData) != 0) //ͨ��һ�����̳�ʼ��ws2_32.dll
	{
		std::cout << "Initialize WSA failed" << std::endl;
		return;
	}

	//�����׽���
	m_slisten = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_slisten == INVALID_SOCKET)
	{
		printf("%d\n", WSAGetLastError());
		printf("%s\n", "Socket Error!");

		return;
	}
	struct sockaddr_in sin;
	std::string bindip;
	sin.sin_family = AF_INET; //���õ�ַ����
	sin.sin_port = htons(port); //���ö˿ںţ�inet_addr("192.168.1.0");
	if (0 == ip[0])
	{
		sin.sin_addr.S_un.S_addr = INADDR_ANY; //���õ�ַ INADDR_ANY:ת����������0.0.0.0����ָ��������˼��Ҳ���Ǳ�ʾ����������IP����Ϊ��Щ���Ӳ�ֹһ��������������������£�����ͱ�ʾ��������ip��ַ����˼��
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
	if (listen(m_slisten, 20) == SOCKET_ERROR)  //�׽���, Ϊ���׽����Ŷӵ����������
												//��ʱ�� slisten ��Ϊ�����׽���
	{
		printf("%s\n", "Listen Error!");
		closesocket(m_slisten); //�رռ����׽���
		WSACleanup(); //��ֹWs2_32.dll��ʹ��
		return;
	}

	std::cout << "bind " << bindip << ":" << port << "\n";
}

void CServerWins::keepClient(Client* cClient)
{
	char revData[1025]; //���ջ���������
	int ret = 0; //���ջ������ֽ���

	while (1)
	{
		ret = recv(cClient->sock, revData, 1024, 0);
		if (ret > 0)
		{
			revData[ret] = '\0';
			std::cout << "Client " << inet_ntoa(cClient->sin_addr)<< "(" << cClient->sock << ")" << " say : " << revData << std::endl;
			//��������  
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

	//�����׽��֣� Ҫ�����������洢λ�ö�Ӧ�ĵ�ַ�� ����
	//closesocket(sClient); //�ر������׽���
	//std::cout << "session end" << std::endl;
	//Sleep(30);
}


void CServerWins::RecMsg()
{
	std::cout << "Rec thread begin ..." << std::endl;
	struct sockaddr_in remoteAddr;     //�洢 ͨ��accept �õ��� �ͻ���IP��ַ
	int nAddrlen = sizeof(remoteAddr); //IP��ַ����
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
	closesocket(m_slisten); //�رռ����׽���
	WSACleanup(); //��ֹWs2_32.dll��ʹ��
}
//��������������������������������
//��Ȩ����������ΪCSDN������csdn_WHB����ԭ�����£���ѭ CC 4.0 BY - SA ��ȨЭ�飬ת���븽��ԭ�ĳ������Ӽ���������
//ԭ�����ӣ�https ://blog.csdn.net/CSDN_WHB/article/details/89160002