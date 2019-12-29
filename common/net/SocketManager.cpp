#include "SocketManager.h"
#include "SocketListener.h"
#include "ODSocket.h"
#include "TcpMessageBuilder.h"
#include "MessageQueue.h"
#include <string>
#include "cocos2d.h"
#include "NetDefine.h"
#include "NetEvent.h"
#include "NetpackHelper.h"

#ifdef WIN32
#include <ws2tcpip.h>
#else
	#include<netdb.h>
#endif

#ifdef WIN32
#define ioctl ioctlsocket

typedef S32 socklen_t;

#endif // WIN32


using namespace std;
USING_NS_CC;



#ifndef MAXPACKETSIZE
#define MAXPACKETSIZE 8196
#endif





enum SocketState
{
	InvalidState,
	Connected,
	ConnectionPending,
	Listening,
	NameLookupRequired
};

struct Socket
{
	Socket()
	{
		fd = InvalidSocket;
		state = InvalidState;
		remoteAddr[0] = 0;
		remotePort = -1;
		isIpV6 = false;
	}
    ~Socket()
    {
        
    }

	NetSocket fd;
	int state;
	char remoteAddr[256];
	int remotePort;
	TcpMessageBuilder mBuilder;
    std::string userdata;
	bool isIpV6;
};

#if defined(WIN32)
static const char* strerror_wsa(int code)
{
	switch (code)
	{
#define E( name ) case name: return #name;
		E(WSANOTINITIALISED);
		E(WSAENETDOWN);
		E(WSAEADDRINUSE);
		E(WSAEINPROGRESS);
		E(WSAEALREADY);
		E(WSAEADDRNOTAVAIL);
		E(WSAEAFNOSUPPORT);
		E(WSAEFAULT);
		E(WSAEINVAL);
		E(WSAEISCONN);
		E(WSAENETUNREACH);
		E(WSAEHOSTUNREACH);
		E(WSAENOBUFS);
		E(WSAENOTSOCK);
		E(WSAETIMEDOUT);
		E(WSAEWOULDBLOCK);
		E(WSAEACCES);
#undef E
default:
	return "Unknown";
	}
}
#endif

class LuaSocketListenerDelegate: public SocketListenerDelegate
{
public:
    ~LuaSocketListenerDelegate(){}
    
    void onSocketEvent(SocketListener* socketListener,int code,const char* data)
    {
        if (NULL != socketListener)
        {
            int nHandler = socketListener->getScriptHandler();
            if (0 != nHandler)
            {
                
                LuaEngine* pEngine = (LuaEngine*)ScriptEngineManager::getInstance()->getScriptEngine();
                LuaStack* luaStack= pEngine->getLuaStack();
                luaStack->pushInt(code);
                luaStack->pushString(data);
                luaStack->executeFunctionByHandler(nHandler, 2);
            }
        }
    }
};

SocketManagerController* SocketManagerController::mInstance = NULL;

SocketManagerController::SocketManagerController()
{
    
	mStartThread = false;
    
}

SocketManagerController::~SocketManagerController()
{
    shutdown();
}

SocketManagerController* SocketManagerController::getInstance()
{
	if (mInstance == NULL)
		mInstance = new SocketManagerController();
	return mInstance;
}
void SocketManagerController::InitSocketThread()
{
	if (mStartThread)
		return;
    
	mStartThread = true;
	pthread_create(&m_netThread, NULL, SocketManager::RunThread, SockMgr);
}
void SocketManagerController::StopSocketThread()
{
		int status = 0;
		SockMgr->setRun(false);
		Sleep(100);
		status = pthread_kill(m_netThread, 0);
		
		if(status == ESRCH){
	        ////CCLOG("\n the thread netThread has exit...\n");
		}
		else if(status == EINVAL)
		{
	        ////CCLOG("\n Send signal to thread netThread fail.\n");
		}
		else
		{
			////CCLOG("\n the thread netThread is still alive.\n");
		}
}

void SocketManagerController::connectionNotifyEvent(Net::ConnectionState state, NetSocket sockid, std::string userdata)
{
	if (state == Net::Connected)
	{
		//if (mReconnected)
		//	mlistener->onReconnectOpen();
		//else
        m_socketListener->onOpen(sockid, userdata);
		//SetConnected(true, sockid);
	}
	else if (state == Net::ConnectFailed || state == Net::DNSFailed)
	{
		m_socketListener->onError(SOCKET_ERROR_CONNECT_FAILED, sockid);
	}
	else if (state == Net::Disconnected)
	{
		m_socketListener->onClose(sockid);
		//SetConnected(false, sockid);
	}
	else if (state == Net::ReconnectedFailed)
	{
		m_socketListener->onReconnectError(SOCKET_ERROR_RECONNECT_FAILED, sockid);
		//SetConnected(false, sockid);
	}


}
void SocketManagerController::PushEvent(NetEvent* pEvent)
{
	m_ToGameThreadQueue.push(pEvent);
}
void SocketManagerController::PushError(int nError, const NetSocket& s)
{
	ErrorNotifyEvent* pEvent = new ErrorNotifyEvent();
	pEvent->nEventError = nError;
    pEvent->sockid = s;
    
	PushEvent(pEvent);
}

void SocketManagerController::PushConnectEvent(int nError, const NetSocket& s, std::string userdata)
{
	ConnectNotifyEvent* pEvent = new ConnectNotifyEvent();
	pEvent->nEventError = nError;
    pEvent->sockid = s;
    pEvent->userdata = userdata;
    
	PushEvent(pEvent);
}

void SocketManagerController::InitNetCtrl(int funcID)
{
    if(m_socketListener == nullptr)
    {
        m_socketListener = new SocketListener();
        LuaSocketListenerDelegate *luaSocketListenerDelegates = new LuaSocketListenerDelegate();
        m_socketListener->setSocketListenerDelegate(luaSocketListenerDelegates);
        m_socketListener->retain();
        
        //m_ToGameThreadQueue.Clear();
    }
    m_socketListener->registerScriptHandler(funcID);
    
    if (!mStartThread)
    {
        InitSocketThread();
        Director::getInstance()->addCGameRunClass(getInstance());

		auto scene = Director::getInstance()->getRunningScene();
    }
}
void SocketManagerController::process()
{
	NetEvent* pEvent = m_ToGameThreadQueue.pop();
	while (pEvent)
	{
		pEvent->Process();
		delete pEvent;
		pEvent = m_ToGameThreadQueue.pop();
	}
    
	if (m_socketListener != nullptr)
	{
		m_socketListener->processRecvBuffCallbacks(0);
		m_socketListener->dispatchResponseCallbacks(0);
	}
}

void SocketManagerController::ProcessError(int nError, const NetSocket& s)
{
    m_socketListener->onError(nError, s);
}

void SocketManagerController::shutdown()
{
	StopSocketThread();
}
void SocketManagerController::Close()
{
	if (m_socketListener != NULL)
    {
        m_socketListener->release();
        delete m_socketListener;
        m_socketListener = nullptr;
		return;
    }
}

void SocketManagerController::SetConnected(bool bFlag, const NetSocket& sockid)
{
    Socket* s = SockMgr->getSocketWithSockId(sockid);
    if(s)
    {
        s->state = Connected;
    }
}
bool SocketManagerController::IsConnect(const NetSocket& sockid)
{
    Socket* s = SockMgr->getSocketWithSockId(sockid);
    if(s)
    {
        return s->state == Connected;
    }
    return false;
}


RawData::RawData()
{
	mData = 0;
	mSize = 0;
    sockid = 0;
}
RawData::~RawData()
{
    release();
}
void RawData::alloc(size_t nSize)
{
	if (mData != NULL)
		delete[] mData;

	mData = new char[nSize];
	memset(mData, 0x0, nSize);
	mSize = nSize;
}

void RawData::alloc(char* buf, size_t nSize)
{
    if(NULL == buf)return;
    
    alloc(nSize);
    memcpy(mData, buf, nSize);
}

void RawData::release()
{
    if (mData != NULL)
        delete mData;
    mData = NULL;
    mSize = 0;
    sockid = 0;
}



bool netSocketWaitForWritable(NetSocket fd, int timeoutMs)
{
	fd_set writefds;
	timeval timeout;

	FD_ZERO(&writefds);
	FD_SET(fd, &writefds);

	timeout.tv_sec = timeoutMs / 1000;
	timeout.tv_usec = (timeoutMs % 1000) * 1000;

	if (select(fd + 1, NULL, &writefds, NULL, &timeout) > 0)
		return true;

	return false;
}

bool SocketManager::mInit = false;
SocketManager::SocketManager()
{
	mSockets = nullptr;
	StartUp();
	mRun = true;

}
void SocketManager::setRun(bool bRun)
{
	mRun = bRun;
}
SocketManager::~SocketManager()
{
	
	if (nullptr != mSockets)
	{
		closeConnectTo(mSockets);
		delete mSockets;
		mSockets = nullptr;
	}
    
    
	Shutdown();
}
void* SocketManager::RunThread(void* data)
{
	SocketManager* sockManager = (SocketManager*)data;
	if (data == NULL)
		return 0;
	while (sockManager->IsRun())
	{
		sockManager->Run();
		Sleep(15);
	}
	return 0;
}
void SocketManager::StartUp()
{
	if (!mInit)
	{
#ifdef WIN32
		WSADATA wsaData;
		WORD version = MAKEWORD(2, 0);
		int ret = WSAStartup(version, &wsaData);//win sock start up
		if (ret)
			return;
#endif
		mInit = true;
	}
}
void SocketManager::Shutdown()
{
#ifdef WIN32
	WSACleanup();
#endif
	return;
}

Socket* SocketManager::addPolledSocket(Socket* s, NetSocket& fd, S32 state,const char* remoteAddr, S32 port)
{
    
    if(s == nullptr)
        return s;
    
    
	
    s->fd = fd;
	s->state = state;
    
	if (remoteAddr)
	{
		int nSize = sizeof(s->remoteAddr);
		memset(s->remoteAddr, 0, nSize);
		int nLen = 128 > nSize ? nSize : 128;
		strncpy(s->remoteAddr, remoteAddr, nLen);
	}
	if (port != -1)
		s->remotePort = port;
	return s;
}

void SocketManager::reconnect(NetSocket sockid, std::string ip, unsigned short port, std::string userdata)
{

    
	if (nullptr != mSockets && mSockets->fd == sockid)
	{
		closeSocket(sockid);
	}
	else
	{
		CCLOG("SocketManager::reconnect -> can't find socket\n");
		return;
	}

    
	Socket* s = mSockets;
	NetSocket resSocket = openConnectTo(s, ip.c_str(), port);
    s->userdata = userdata;
    
	if (resSocket != InvalidSocket)
	{
		s->remotePort = port;
		s->fd = resSocket;
        
		int nSize = sizeof(s->remoteAddr);
		memset(s->remoteAddr, 0, nSize);
		strcpy(s->remoteAddr, ip.c_str());

		//CCLOG("SocketManager::reconnect -> succ ip=%s port=%d\n", ip.c_str(), port);

	}
	else
	{
		//CCLOG("SocketManager::reconnect -> failed ip=%s port=%d\n", ip.c_str(), port);
		SockController->PushConnectEvent(Net::ReconnectedFailed, sockid, s->userdata);

	}
}

bool SocketManager::reconnect(NetSocket sockid, std::string userdata)
{
	if (nullptr != mSockets && mSockets->fd == sockid)
	{
		reconnect(sockid, mSockets->remoteAddr, mSockets->remotePort, userdata);
		return true;
	}
	return false;
}

void SocketManager::disconnect(NetSocket sockid)
{
    closeSocket(sockid);
}

void checkIpV6(const char* domain, std::string& ip)
{
	char	strIPBuffer[128];
	memset(strIPBuffer, 0, sizeof(strIPBuffer));
	struct addrinfo *answer, hint, *curr;
	memset(&hint, 0, sizeof(hint));
	hint.ai_family = AF_UNSPEC;
	hint.ai_socktype = SOCK_STREAM;
	int ret = getaddrinfo(domain, NULL, &hint, &answer);
	if (ret != 0) {
		return ;
	}
	for (curr = answer; curr != NULL; curr = curr->ai_next) {
		getnameinfo(curr->ai_addr, curr->ai_addrlen, strIPBuffer, sizeof(strIPBuffer), 0, 0, 0);
		switch (curr->ai_family){
		case AF_UNSPEC:
			break;
		case AF_INET:
			return ;
		case AF_INET6:
			ip.assign(strIPBuffer);
			return ;
		}
	}
}


bool SocketManager::createtcp(const char* pIp, unsigned short pPort, const char* domain, const std::string& userdata)
{
	if (nullptr != mSockets)
	{
		closeSocket(mSockets->fd);
		delete mSockets;
		mSockets = nullptr;
	}

	Socket* s = new Socket();
	if (nullptr == s) return false;
    
    static NetSocket skey = 1;
    static const NetSocket maxkey = -1;
    do
    {
        ++skey;
        if(skey >= ~maxkey) skey = 1;
		s->fd = skey;
		break;

    }while (true);    
    s->userdata = userdata;

	mSockets = s;
    

	string strIp(pIp);
	unsigned short port = pPort;
	bool isIpV6=false;
	if (strIp.empty())
	{
		checkIpV6(domain, strIp);
		if (strIp.empty())
		{
			struct hostent *hp;
			struct in_addr in;
			hp = gethostbyname(domain);
			if (!hp)
			{
				SockController->PushError(SOCKET_ERROR_DOMAIN_PARSE_FAILED, s->fd);

				//pListener->onError(SOCKET_ERROR_DOMAIN_PARSE_FAILED);
				return false;
			}

			memcpy(&in.s_addr, hp->h_addr, 4);
			strIp = inet_ntoa(in);

			//CCLOG("ip=%s port=%d\n", strIp.c_str(), port);
		}
		else
		{
			isIpV6 = true;
		}
	}
	
	NetSocket resSocket;
	s->isIpV6 = isIpV6;
	if (isIpV6)
	{
		resSocket = openConnectToIpV6(s, strIp.c_str(), port);
	}
	else
	{
		resSocket = openConnectTo(s, strIp.c_str(), port);
	}
	

	s->remotePort = port;
	s->fd = resSocket;

	int nSize = sizeof(s->remoteAddr);
	memset(s->remoteAddr, 0, nSize);
	strcpy(s->remoteAddr, strIp.c_str());

    

	if (resSocket == InvalidSocket)
	{
		//CCLOG("SocketManager::init -> failed ip=%s port=%d\n", strIp.c_str(), port);
		SockController->PushError(SOCKET_ERROR_CONNECT_FAILED, s->fd);
		//pListener->onError(SOCKET_ERROR_CONNECT_FAILED);
		return false;
	}
	return true;
}


void SocketManager::Run()
{
	ProcessEvent();
    
    
	int  optval;
	socklen_t optlen = sizeof(int);
	int bytesRead;
	Net::Error err;
	NetSocket incoming = InvalidSocket;
	int out_h_length = 0;

	
	do
	{
		if (nullptr == mSockets)break;
		Socket* s = mSockets;

		switch (s->state)
		{
		case ::InvalidState:
			CCLOG("\n Error, InvalidState socket in polled sockets  list\n");
			break;

		case ::ConnectionPending:
			// see if it is now connected
			//#ifdef TORQUE_OS_XENON
			//			// WSASetLastError has no return value, however part of the SO_ERROR behavior
			//			// is to clear the last error, so this needs to be done here.
			//			if ((optval = _getLastErrorAndClear()) == -1)
			//#else
			if (getsockopt(s->fd, SOL_SOCKET, SO_ERROR,
				(char*)&optval, &optlen) == -1)
				//#endif
			{
				//CCLOG("Error getting socket options: %s", strerror(errno));
				SockController->PushConnectEvent(Net::ConnectFailed, s->fd, s->userdata);
				s->state = ::InvalidState;
			}
			else
			{
				if (optval == EINPROGRESS)// still connecting...
					continue;

				if (optval == 0)
				{
					// poll for writable status to be sure we're connected.
					bool ready = netSocketWaitForWritable(s->fd, 0);
					if (!ready)
						continue;

					s->state = ::Connected;
					SockController->PushConnectEvent(Net::Connected, s->fd, s->userdata);
				}
				else
				{
					// some kind of error
					//CCLOG("\nError connecting: %s\n", strerror(errno));
					SockController->PushConnectEvent(Net::ConnectFailed, s->fd, s->userdata);
					s->state = ::InvalidState;
				}
			}
			break;
		case ::Connected:
		{
			// try to get some data
			bytesRead = 0;
			static unsigned char tmpbuff[MAXPACKETSIZE] = { 0 };

			tmpbuff[0] = 0;
			err = recv(s->fd, tmpbuff, MAXPACKETSIZE - 1, &bytesRead);
			if (err == Net::NoError)
			{
				if (bytesRead > 0)
				{
					// got some data, post it
					addRecvBuff((char*)tmpbuff, bytesRead, s->fd);
				}
				else
				{
					// ack! this shouldn't happen
					//if (bytesRead < 0)
					//CCLOG("\nUnexpected error on socket: %s\n", strerror(errno));

					CCLOG("Net::Disconnected1: %d,%d", bytesRead, s->fd);
					// zero bytes read means EOF
					SockController->PushConnectEvent(Net::Disconnected, s->fd, s->userdata);
					s->state = ::InvalidState;

				}
			}
			else if (err != Net::NoError && err != Net::WouldBlock)
			{
				//CCLOG("\nError reading from socket: %s\n", strerror(errno));
				CCLOG("Net::Disconnected2: %d,%d", err, s->fd);
				SockController->PushConnectEvent(Net::Disconnected, s->fd, s->userdata);
				s->state = InvalidState;
			}
		}break;
		case SocketState::NameLookupRequired:
			continue;
		}
	} while (false);

		
	 
	if (nullptr != mSockets && mSockets->state == ::InvalidState)
	{
		if (nullptr != mSockets)
		{
			closeConnectTo(mSockets);
			delete mSockets;
			mSockets = nullptr;
		}
	}
}

void SocketManager::closeConnectTo(Socket* s)
{

	if (s == NULL)
		return;

	if (s->fd!=InvalidSocket)
		closeSocket(s->fd);
    
	s->fd = InvalidSocket;
    s->state = InvalidState;
	s->mBuilder.Close();
}

Net::Error SocketManager::closeSocket(NetSocket sock)
{
	CCLOG("SocketManager::closeSocket: %d", sock);
	if (sock != InvalidSocket)
	{
		if (!closesocket(sock))
			return Net::Error::NoError;
		else
			return getLastError();
	}
	else
		return Net::Error::NotASocket;
}

Net::Error SocketManager::getLastError()
{
#if defined(WIN32)
	int  err = WSAGetLastError();
	if (err != 10035)CCLOG("WSAGetLastError:%d", err);
	switch (err)
	{
	case 0:
		return Net::NoError;
	case WSAEWOULDBLOCK:
		return Net::WouldBlock;
	default:
		return Net::UnknownError;
	}
#else
	int err = errno;
	
	if (err == EAGAIN || err == EINPROGRESS || err==EALREADY || err==EINTR || err==EISCONN)
		return Net::WouldBlock;
	if (err == 0)
		return Net::NoError;
	CCLOG("getLastError:%d", err);
	return Net::UnknownError;
#endif
}


void SocketManager::addRecvBuff(char* buff, const size_t& buflen, const NetSocket& sid)
{
	RawData* d = new RawData();
    d->sockid = sid;
    d->alloc(buff, buflen);
    /*
	//test code
	static std::vector<RawData*> vtrlist;
	if (vtrlist.size() == 2)
	{
		RawData* a1 = vtrlist[0];
		RawData* a2 = vtrlist[1];
		
		size_t remainsz = a1->mSize/2;
		a1->mSize -= remainsz;
		char* tmpbuf = new char[a2->mSize + remainsz];
		memcpy(tmpbuf, a1->mData+a1->mSize, remainsz);
		memcpy(tmpbuf+remainsz, a2->mData, a2->mSize);
		delete a2->mData;
		a2->mData = tmpbuf;
		a2->mSize += remainsz;
		
		addRecvBuffToRecvQueue(a1);
		addRecvBuffToRecvQueue(a2);

		vtrlist.clear();
	}
	else
	{
		vtrlist.push_back(d);
	}
	*/
	addRecvBuffToRecvQueue(d);
//version 2
	/*
	const size_t max_st_size = 4096;
	char *stTData = new char[max_st_size];
	memcpy(stTData, data.mData, data.mSize);
	stTData[data.mSize] = 0;

	Message m;
	m.code = MESSAGE_TYPE_RECEIVE_NEW_MESSAGE;
	m.len = data.mSize;
	m.message = stTData;
	m.free = true;
	//m.message = "newmsg";
	//m.dict = NetpackHelper::bufToLuaValue(message->data, message->length);
	SockMgr->addMessageToReceiveQueue(m);//Ω´œ˚œ¢∑≈µΩ¥˝¥¶¿Ì∂”¡–
	*/
	
//version 1
// 	TcpMessage* message = sock->mBuilder.buildMessage();
// 	while (message)
// 	{
// 		if (message->length == 0)
// 		{
// 			Message m;
// 			m.code = MESSAGE_TYPE_RECEIVE_HEART_BEAT;
// 			m.message = "heart beat";
// 			SockMgr->addMessageToReceiveQueue(m);//Ω´œ˚œ¢∑≈µΩ¥˝¥¶¿Ì∂”¡–
// 		}
// 		else
// 		{
// 
// 			Message m;
// 			m.code = MESSAGE_TYPE_RECEIVE_NEW_MESSAGE;
// 			m.message = message->data;
// 			//m.message = "newmsg";
// 			//m.dict = NetpackHelper::bufToLuaValue(message->data, message->length);
// 			SockMgr->addMessageToReceiveQueue(m);//Ω´œ˚œ¢∑≈µΩ¥˝¥¶¿Ì∂”¡–
// 
// 			//if (mlistener!=NULL)
// 				//mlistener->onReceiveNewData(message->data, message->length);
// 		}
// 		delete message;
// 		message = NULL;
// 		message = sock->mBuilder.buildMessage();
// 
 }


Net::Error SocketManager::recv(NetSocket socket, U8 *buffer, int bufferSize, int  *bytesRead)
{
	*bytesRead = ::recv(socket, (char*)buffer, bufferSize, 0);
	if (*bytesRead == -1)
		return getLastError();
	return Net::Error::NoError;
}



NetSocket SocketManager::openConnectTo(Socket* s, const char *addressString, U16 nPort)
{
	if (s == nullptr)
		return InvalidSocket;

	if (s->isIpV6)
	{
		return openConnectToIpV6(s, addressString, nPort);
	}
		

	U16 port = htons(nPort);

	NetSocket sock = openSocket(false);
	if (sock != InvalidSocket && Net::Error::NoError != setBlocking(sock, false))
	{
		return InvalidSocket;
	}

	sockaddr_in ipAddr;
	memset(&ipAddr, 0, sizeof(ipAddr));
	ipAddr.sin_addr.s_addr = inet_addr(addressString);

	if (ipAddr.sin_addr.s_addr != INADDR_NONE)
	{
		ipAddr.sin_port = port;
		ipAddr.sin_family = AF_INET;
		if (::connect(sock, (struct sockaddr *)&ipAddr, sizeof(ipAddr)) == -1)
		{
			int err = getLastError();
			if (err != Net::WouldBlock)
			{
				////CCLOG("Error connecting %s: %s",addressString, strerror(err));
				::closesocket(sock);
				sock = InvalidSocket;
			}
			else if (err != Net::NoError)
			{
				//#if CC_TARGET_PLATFORM != CC_PLATFORM_WIN32
				int nNetTimeout = 10000;//haomiao
				//send timeout
				::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&nNetTimeout, sizeof(int));
				//recv timeout
				::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&nNetTimeout, sizeof(int));
				//#endif
			}
		}
		if (sock != InvalidSocket)
		{
			// add this socket to our list of polled sockets
			addPolledSocket(s, sock, ConnectionPending);
		}
	}
	else
	{
		// need to do an asynchronous name lookup.  first, add the socket
		// to the polled list
		addPolledSocket(s, sock, NameLookupRequired, addressString, port);
		// queue the lookup
		//gNetAsync.queueLookup(remoteAddr, sock);
	}

	return sock;
}

NetSocket SocketManager::openConnectToIpV6(Socket* s, const char *addressString, U16 nPort)
{

#if defined(WIN32)
	return 0;
#else
	if (s == nullptr)
		return InvalidSocket;

	U16 port = htons(nPort);

	NetSocket sock = openSocket(true);
	if (sock != InvalidSocket && Net::Error::NoError != setBlocking(sock, false))
	{
		return InvalidSocket;
	}

	sockaddr_in6 ipAddr;
	memset(&ipAddr, 0, sizeof(ipAddr));
	ipAddr.sin6_family = AF_INET6;
	//ipAddr.sin6_addr = in6addr_any;
	ipAddr.sin6_port = htons(nPort);
	#if (CC_TARGET_PLATFORM != CC_PLATFORM_ANDROID)
	ipAddr.sin6_len = sizeof(struct sockaddr_in6);
	#endif

	int nRet = inet_pton(AF_INET6, addressString, &ipAddr.sin6_addr);
	if (nRet != INADDR_NONE)
	{
		if (::connect(sock, (struct sockaddr *)&ipAddr, sizeof(sockaddr_in6)) == -1)
		{
			int err = getLastError();
			if (err != Net::WouldBlock)
			{
				////CCLOG("Error connecting %s: %s",addressString, strerror(err));
				::closesocket(sock);
				sock = InvalidSocket;
			}
			else if (err != Net::NoError)
			{
				//#if CC_TARGET_PLATFORM != CC_PLATFORM_WIN32
				int nNetTimeout = 10000;//haomiao
				//send timeout
				::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&nNetTimeout, sizeof(int));
				//recv timeout
				::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&nNetTimeout, sizeof(int));
				//#endif
			}
		}
		if (sock != InvalidSocket)
		{
			// add this socket to our list of polled sockets
			addPolledSocket(s, sock, ConnectionPending);
		}
	}
	else
	{
		// need to do an asynchronous name lookup.  first, add the socket
		// to the polled list
		addPolledSocket(s, sock, NameLookupRequired, addressString, port);
		// queue the lookup
		//gNetAsync.queueLookup(remoteAddr, sock);
	}

	return sock;
#endif
}


NetSocket SocketManager::openSocket(bool isIpV6)
{
	NetSocket retSocket;
	retSocket = socket(isIpV6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0);

	if (retSocket == InvalidSocket)
		return InvalidSocket;
	else
		return retSocket;
}


Net::Error SocketManager::setBlocking(NetSocket socket, bool blockingIO)
{
	unsigned long notblock = !blockingIO;
	S32 error = ioctl(socket, FIONBIO, &notblock);
	if (!error)
		return Net::Error::NoError;
	return getLastError();
}

Net::Error SocketManager::send(NetSocket socket, const U8 *buffer, S32 bufferSize)
{
	errno = 0;
	S32 bytesWritten = ::send(socket, (const char*)buffer, bufferSize, 0);
	CCLOG("SocketManager::send: %d, %d, %s", bytesWritten, bufferSize, buffer);
	if (bytesWritten == -1)
#if defined(WIN32)
		////CCLOG("\nCould not write to socket. Error: %s\n", strerror_wsa(WSAGetLastError()));
#else
		////CCLOG("\nCould not write to socket. Error: %s\n", strerror(errno));
#endif

	return getLastError();
}

Net::Error SocketManager::sendByteBuf(ByteBuffer buf, NetSocket sockid)
{
	//if (mCurrSocket == NULL)
	//	return Net::UnknownError;

	return send(sockid,buf.contents(), buf.size());
}
Net::Error SocketManager::sendByteBuf(const char* buf, const size_t& len, NetSocket sockid)
{
	//if (mCurrSocket == NULL)
	//	return Net::UnknownError;

	return send(sockid,(const U8*)buf, len);
}

bool SocketManager::isConnected(NetSocket sockid)
{
	//if (mCurrSocket == NULL || mCurrSocket->fd == InvalidSocket)
	//	return false;
    
	if (nullptr != mSockets && mSockets->fd == sockid)
	{
		return mSockets->state == Net::Connected;
	}
  
    return false;
	

}
void SocketManager::addMessageToReceiveQueue(Message m)
{
	mRespQueue.push(m);
}
void SocketManager::addRecvBuffToRecvQueue(RawData* buf)
{
	mRecvBuffQueue.push(buf);
}
Message SocketManager::getAndRemoveMessageFromReceiveQueue()
{
	Message m = mRespQueue.pop();
	return m;
}
RawData* SocketManager::getAndPopRecvBuffFromRecvQueue()
{
	RawData* buf = mRecvBuffQueue.pop();
	return buf;
}
RawData* SocketManager::getRecvBuffFromRecvQueue()
{
	RawData* buf = mRecvBuffQueue.top();
	return buf;
}
void SocketManager::addRecvBuffStickToRecvQueue(RawData* buf)
{
	mRecvBuffQueue.pushStick(buf);
}
RawData* SocketManager::getAndRemoveRecvBuffStickFromRecvQueue()
{
	RawData* buf = mRecvBuffQueue.popStick();
	return buf;
}
RawData* SocketManager::getRecvBuffStickFromRecvQueue()
{
	RawData* buf = mRecvBuffQueue.topStick();
	return buf;
}

bool SocketManager::IsRun()
{
	return mRun;
}

void SocketManager::ProcessEvent()
{
	NetEvent* pEvent = m_ToNetThreadQueue.pop();
	while (pEvent)
	{
		pEvent->Process();
		pEvent = m_ToNetThreadQueue.pop();
	}
}
void SocketManager::PushEvent(NetEvent* pEvent)
{
	m_ToNetThreadQueue.push(pEvent);
}

Socket* SocketManager::getSocketWithSockId(const NetSocket& sockid)
{
	return mSockets;
}


