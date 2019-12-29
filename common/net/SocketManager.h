#ifndef __SOCKET_MANAGER_EX_H__
#define __SOCKET_MANAGER_EX_H__
#include <string>
#include <vector>
#include "Singleton.h"
#include "SocketListener.h"
#include "ODSocket.h"
#include "pthread.h"
#include "MessageQueue.h"
#include "signal.h"
#include "stdio.h"
#include "ByteBuffer.h"
#include "game/GameRunClassManager.h"
#include "NetDefine.h"

#define SOCKET_RECV_BUF_LEN (1024)		//socket buffer size
#define SOCKET_ERROR_CONNECT_FAILED (1) //socket connect failed
#define SOCKET_ERROR_RECONNECT_FAILED (3) //socket reconnect failed
#define SOCKET_ERROR_DOMAIN_PARSE_FAILED (2) //host parse failed

#define HEART_BEAT_INTERVAL (1000*60*1)		//beatheart time

#define SOCKET_MESSAGE_MAX_LEN 4096//msg max len

namespace Net
{
	enum Error
	{
		NoError,
		WrongProtocolType,
		InvalidPacketProtocol,
		WouldBlock,
		NotASocket,
		UnknownError
	};

	enum ConnectionState {
		DNSResolved,
		DNSFailed,
		Connected,
		ConnectFailed,
		Disconnected,
		ReconnectedFailed,
	};

	enum Protocol
	{
		UDPProtocol,
		TCPProtocol
	};
};



struct NetAddress
{
	int type;        ///< Type of address (IPAddress currently)

	/// Acceptable NetAddress types.
	enum
	{
		IPAddress,
	};

	unsigned char netNum[4];    ///< For IP:  sin_addr<br>
	unsigned char nodeNum[6];   ///< For IP:  Not used.<br>
	unsigned short  port;       ///< For IP:  sin_port<br>
};



typedef unsigned int		U32;
typedef unsigned short		U16;
typedef unsigned char		U8;
typedef int					S32;
struct Socket;

class SocketListener;

class SocketManagerController : public Singleton<SocketManagerController>
{
public:
	SocketManagerController();
    ~SocketManagerController();

	void InitSocketThread();
	void StopSocketThread();


    void connectionNotifyEvent( Net::ConnectionState state, NetSocket sockid, std::string userdata);
	void PushEvent(NetEvent* pEvent);

	void PushError(int nError, const NetSocket& s);
	void ProcessError(int nError, const NetSocket& s);
	void PushConnectEvent(int nError, const NetSocket& s, std::string userdata);
	

    void InitNetCtrl(int funcID);

	virtual void process();
	virtual void shutdown();
	void Close();
	void SetConnected(bool bFlag, const NetSocket& sockid);
	bool IsConnect(const NetSocket& sockid);

	static SocketManagerController* getInstance();

    

	pthread_t m_netThread;

	AsyncNetEvent m_ToGameThreadQueue;
    SocketListener* m_socketListener = nullptr;
	bool mStartThread;
	static SocketManagerController* mInstance;
	
};
#define SockController  SocketManagerController::getInstance()






class SocketManager :public Singleton<SocketManager>
{
public:
	SocketManager();
	virtual ~SocketManager();


	
	// Only the following function support synchronization, can cross thread calls
	void addMessageToReceiveQueue(Message m);
	Message getAndRemoveMessageFromReceiveQueue();
	
	void addRecvBuffToRecvQueue(RawData* buf);
	RawData* getAndPopRecvBuffFromRecvQueue();
	RawData* getRecvBuffFromRecvQueue();

	void addRecvBuffStickToRecvQueue(RawData* buf);
	RawData* getAndRemoveRecvBuffStickFromRecvQueue();
	RawData* getRecvBuffStickFromRecvQueue();

	void ProcessEvent();
	void PushEvent(NetEvent* pEvent);

	



	static void* RunThread(void* data);
	void Run();

	void StartUp();
	void Shutdown();


	

	void closeConnectTo(Socket* s);
	Net::Error closeSocket(NetSocket sock);
	Net::Error getLastError();

	void addRecvBuff(char* buff, const size_t& buflen, const NetSocket& sid);
	Net::Error recv(NetSocket socket, U8 *buffer, int bufferSize, int  *bytesRead);
	NetSocket openConnectTo(Socket* s, const char *addressString, U16 nPort);
	NetSocket openConnectToIpV6(Socket* s, const char *addressString, U16 nPort);

	Socket* addPolledSocket(Socket* s, NetSocket& fd, S32 state, const char* remoteAddr = NULL, S32 port = -1);

	NetSocket openSocket(bool isIpV6);
	Net::Error setBlocking(NetSocket socket, bool blockingIO);


	Net::Error send(NetSocket socket, const U8 *buffer, S32 bufferSize);
	Net::Error sendByteBuf(ByteBuffer buf, NetSocket sockid);
	Net::Error sendByteBuf(const char* buf, const size_t& len, NetSocket sockid);

	void reconnect(NetSocket sockid, std::string ip, unsigned short port, std::string userdata);
	bool reconnect(NetSocket sockid, std::string userdata);//socket reconnect

	void disconnect(NetSocket sockid);

	bool createtcp(const char* pIp, unsigned short pPort, const char* domain, const std::string& userdata);


	bool isConnected(NetSocket sockid);


	bool IsRun();
	void setRun(bool bRun);
	
    Socket* getSocketWithSockId(const NetSocket& sockid);

private:
	AsyncRecvBuffQueue mRecvBuffQueue;

	AsyncMessageQueue mRespQueue;

	AsyncNetEvent m_ToNetThreadQueue;
	
	//Socket* mCurrSocket;
    //std::vector<Socket*> mSockets;
	Socket* mSockets;
	static bool mInit;

	
	bool mRun;



};
#define  SockMgr			SocketManager::getInstance()

#endif
