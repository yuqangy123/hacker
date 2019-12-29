#pragma once
#include <winsock2.h>
#include <vector>
#include <mutex>

using namespace std;

struct Client
{
	SOCKET sock;
	USHORT sin_port;
	IN_ADDR sin_addr;

	Client(SOCKET s, IN_ADDR addr, USHORT port):sock(s), sin_addr(addr), sin_port(port) {};
};

class CServerWins
{
public:
	CServerWins(const char* ip, int port);
	~CServerWins();

	void RecMsg();
	void stopRec();

protected:
	void keepClient(Client& sClient);

private:
	bool m_terminal;
	SOCKET m_slisten; //���������׽���
	vector<Client> m_clients;
	mutex m_io_mutex;
};

