#pragma once

//#include "stdio.h"
#include <winsock2.h>
#include <map>

//#pragma comment(lib, "Ws2_32.lib")

#define MAX_BUFFER        8192//1024//
#define SERVER_IP        "127.0.0.1"
#define SERVER_PORT        3500

#define FLAG_RECV 0
#define FLAG_SEND 1

//struct SOCKETINFO
//{
//	WSAOVERLAPPED overlapped;
//	WSABUF dataBuffer;
//	SOCKET socket;
//	char messageBuffer[MAX_BUFFER];
//	int receiveBytes;
//	int sendBytes;
//	DWORD flags;
//};

/////////////////////////////////////////////////////////////////
class ClientSession;

enum IO_OPERATION
{
	ClientIoRead = 3, // 읽기 작업 진행 중 	
	ClientIoWrite = 4,  // 쓰기 작업 진행 중 
};

struct OVERLAPPED_EX : public WSAOVERLAPPED
{
	IO_OPERATION	ioType;
	ClientSession*	clientSession;
	void Reset()
	{
		ioType = IO_OPERATION::ClientIoRead;
		clientSession = nullptr;
	}
};

struct IO_CONTEXT
{
	OVERLAPPED_EX readOverlapped;
	OVERLAPPED_EX sendOverlapped;
	char readBuffer[MAX_BUFFER];
	void Reset()
	{
		readOverlapped.Reset();
		sendOverlapped.Reset();
		memset(&readBuffer, 0, sizeof(readBuffer));
	}

	void Initialize(ClientSession* clientSession)
	{
		memset(&readOverlapped, 0, sizeof(OVERLAPPED_EX));
		readOverlapped.ioType = IO_OPERATION::ClientIoRead;
		readOverlapped.clientSession = clientSession;

		memset(&sendOverlapped, 0, sizeof(OVERLAPPED_EX));
		sendOverlapped.ioType = IO_OPERATION::ClientIoWrite;
		sendOverlapped.clientSession = clientSession;

		memset(&readBuffer, 0, sizeof(readBuffer));
	}
};

class ClientSession
{
public:
	SOCKET socket;
	IO_CONTEXT ioContext;
	bool connected;

	ClientSession()
		: socket(0)
		, connected(false)
	{
		ioContext.Initialize(this);
	}
	virtual ~ClientSession()
	{
		socket = 0;
		ioContext.Reset();
		connected = false;
	}

	void OnConncted()
	{
		connected = true;
	}

	void OnDisconncted()
	{
		closesocket(socket);
		ioContext.Reset();
		connected = false;
	}
};

class ClientSessionManager
{
private:
	typedef std::map<int, ClientSession*> ClientSessionMap;

private:
	ClientSessionMap clientSessions;

public:
	ClientSessionManager() {}
	~ClientSessionManager()
	{
		for (auto iter = clientSessions.begin(); iter != clientSessions.end(); ++iter)
		{
			delete iter->second;
		}
		clientSessions.clear();
	}
};


void OnDisconncted(ClientSession* clientSession)
{
	if (!clientSession) return;
	clientSession->OnDisconncted();
}


//8/21 해야할일///////////////////////////////////////////////////////////////
//github
//readbuff,writebuff
//accept관리
//파일로드
//엘라스틱서치
//웹통신

//1) 패킷처리
//2) DB처리(쓰레드 따로 만들기)