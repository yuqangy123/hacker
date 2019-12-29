#include "stdafx.h"
#include <iostream>
#include "ServerWins.h"
#include <thread>


using namespace std;

void usage()
{
	printf("\t===================ServerWins======================\n");
	printf("\t1.启用本机的所有IP（默认）\n");
	printf("\t2.指定绑定IP\n");
}

int main(int argc, char* argv[])
{
	usage();
	char input_buf[128] = { 0 };
	cin >> input_buf;
	
	char ip[32] = { 0 };

	int port = 0;

	switch (input_buf[0])
	{
		case '2':
		{
			cout << "请输入IP：";
			cin >> ip;

		}break;

		case '1':
		default:
		{
		}break;
	}
	cout << "请输入端口：";
	cin >> input_buf;
	port = atoi(input_buf);

	CServerWins server(ip, port);

	thread rec(&CServerWins::RecMsg, &server);
	rec.join();

	std::cout << "I love China!\n";

	return 0;
}

