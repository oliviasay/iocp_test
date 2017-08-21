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



////3차완료/////////////////////////////////////////////////////////////////
//struct OVERLAPPED_EX
//{
//	WSAOVERLAPPED	overlapped;
//	IO_OPERATION	ioType;
//	ClientSession*	clientSession;
//	void Reset()
//	{
//		memset(&overlapped, 0, sizeof(WSAOVERLAPPED));
//		ioType = IO_OPERATION::ClientIoRead;
//		clientSession = nullptr;
//	}
//};

////2차완료/////////////////////////////////////////////////////////////////
//typedef enum _IO_OPERATION
//{
//	ClientIoRead, // 읽기 작업 진행 중 
//	ClientIoWrite  // 쓰기 작업 진행 중 
//} IO_OPERATION, *PIO_OPERATION;
//
//struct IO_CONTEXT
//{
//	WSAOVERLAPPED overlapped;
//	WSABUF dataBuffer;
//	char messageBuffer[MAX_BUFFER];
//	int receiveBytes;
//	int sendBytes;
//	IO_OPERATION IOOperation;
//};
//
//struct ClientSession
//{
//	SOCKET socket;
//	IO_CONTEXT* ioContext;
//};



















///////////////////////////////////////////////////////////////////////////////
//
//// IOCP와 연관되는 소켓마다 할당되는 구조체 
//typedef struct _PER_SOCKET_CONTEXT
//{
//	SOCKET                 Socket;
//	PPER_IO_CONTEXT       pIOContext;
//} PER_SOCKET_CONTEXT, *PPER_SOCKET_CONTEXT;
//
//위에서 Socket은 클라이언트가 하나 연결될 때마다 부여되는 소켓이다.pIOContext는 이 소켓과의 입출력 작업에 사용되는 메모리 버퍼와 각종 구조체를 모아둔 구조체로 이 소켓내에서 벌어지는 입출력 작업의 상태를 나타낸다고 생각하면 된다.다음과 같이 정의되어 있다.
//
//#define MAX_BUFF_SIZE       8192 
//// 소켓에 대한 입출력 작업에 사용되는 구조체 
//typedef struct _PER_IO_CONTEXT
//{
//	WSAOVERLAPPED        Overlapped;
//	char                     Buffer[MAX_BUFF_SIZE];
//	WSABUF                 wsabuf;
//	int                       nTotalBytes;
//	int                       nSentBytes;
//	IO_OPERATION           IOOperation;
//} PER
//
//
//DWORD WINAPI EchoThread(LPVOID WorkThreadContext)
//{
//	// 앞서 스레드 생성시 스레드 함수의 인자로 IOCP 핸들을 지정했었다. 
//	// 인자를 IOCP 핸들로 캐스팅한다. 
//	HANDLE hIOCP = (HANDLE)WorkThreadContext;
//	BOOL   bSuccess = FALSE;
//	int      nRet;
//	LPOVERLAPPED    lpOverlapped = NULL;
//	PPER_SOCKET_CONTEXT lpPerSocketContext = NULL;
//	PPER_IO_CONTEXT     lpIOContext = NULL;
//	WSABUF buffRecv;
//	WSABUF buffSend;
//	DWORD  dwRecvNumBytes = 0;
//	DWORD  dwSendNumBytes = 0;
//	DWORD  dwFlags = 0;
//	DWORD  dwIoSize;
//	while (TRUE)
//	{
//		// IOCP 큐에서 비동기 I/O 결과를 하나 읽어온다. 
//		bSuccess = GetQueuedCompletionStatus(hIOCP, &dwIoSize,
//			(LPDWORD)&lpPerSocketContext, &lpOverlapped, INFINITE);
//		if (!bSuccess)
//			printf("GetQueuedCompletionStatus: %d\n", GetLastError());
//
//		// CleanUp 함수에 의해서 스레드의 강제 종료 명령이 내려지면.. 
//		if (lpPerSocketContext == NULL)  return 0;
//		if (g_bEndServer) return 0;
//		// 클라이언트와의 소켓 연결이 끊어졌으면… 
//		if (!bSuccess || (bSuccess && (0 == dwIoSize)))
//		{
//			// lpPerSocketContext를 메모리에서 제거한다. 
//			CloseClient(lpPerSocketContext);
//			continue;
//		}
//
//		/* 앞서 WSASend와 WSARecv에 의해 I/O 작업을 할 때 넘겼던 WSAOVERLAPPED
//		타입의 변수가 사실은 PER_IO_CONTEXT 타입의 시작이기도 하므로 이를 캐스팅하
//		여 사용가능하다. */
//		lpIOContext = (PPER_IO_CONTEXT)lpOverlapped;
//		switch (lpIOContext->IOOperation) // 끝난 작업 종류가 무엇인가 ? 
//		{
//		case ClientIoRead: // 읽기 작업인가 ? 
//						   // -------------------------------------------- 
//						   // 받은 것을 그대로 보낸다. 즉, 다음 작업은 쓰기 작업이다. 
//						   // -------------------------------------------- 
//			printf("%s를 받았고 이를 재전송합니다.\n.", lpIOContext->wsabuf.buf);
//			lpIOContext->IOOperation = ClientIoWrite; // 이제 쓰기 작업이 진행됨을 표시 
//													  // 얼마큼 전송할 것인지 명시한다. 받은 만큼 보낸다. 이는 상태를 기록하기 
//													  // 위함이지 WSASend 함수와는 관련없다. 
//			lpIOContext->nTotalBytes = dwIoSize;
//			// 전송된 데이터 크기. 아직 보낸 것이 없으므로 0 
//			lpIOContext->nSentBytes = 0;
//			// WSASend에게 보낼 데이터의 포인터와 크기를 지정한다. 
//			// 받은 데이터가 이미 lpIOContext->wsabuf.buf에 있다. 
//			lpIOContext->wsabuf.len = dwIoSize; // 크기 지정 
//			dwFlags = 0;
//			nRet = WSASend(lpPerSocketContext->Socket,
//				&lpIOContext->wsabuf, 1, &dwSendNumBytes,
//				dwFlags, &(lpIOContext->Overlapped), NULL);
//			if (SOCKET_ERROR == nRet && (ERROR_IO_PENDING != WSAGetLastError()))
//			{
//				printf("WSASend: %d\n", WSAGetLastError());
//				CloseClient(lpPerSocketContext);
//			}
//			break;
//
//		case ClientIoWrite: // 쓰기 작업인가 ? 
//							// ---------------------------------------------------- 
//							// 전송이 다 되었는지 확인한다. 다 전송되지 않았으면 아직 전송되지 
//							// 않은 데이터를 다시 보낸다. 다 전송되었으면 WSARecv를 호출해서 
//							// 다시 받기 모드로 진입한다.  
//							// -------------------------------------------- 
//			lpIOContext->nSentBytes += dwIoSize; // 전송된 데이터 크기 업데이트 
//			dwFlags = 0;
//			if (lpIOContext->nSentBytesnTotalBytes) // 다 전송되지 않았으면 
//			{
//				// 마저 전송해야 하므로 아직 보내기모드 
//				lpIOContext->IOOperation = ClientIoWrite;
//				// ----------------------- 
//				// 전송되지 않은 부분을 보낸다. 
//				// ----------------------- 
//				// 버퍼 포인터를 업데이트하고 
//				buffSend.buf = lpIOContext->Buffer + lpIOContext->nSentBytes;
//				// 보내야할 데이터의 크기를 남은 데이터의 크기만큼으로 줄인다. 
//				buffSend.len = lpIOContext->nTotalBytes - lpIOContext->nSentBytes;
//				nRet = WSASend(lpPerSocketContext->Socket,
//					&buffSend, 1, &dwSendNumBytes,
//					dwFlags, &(lpIOContext->Overlapped), NULL);
//				// SOCKET_ERROR가 리턴된 경우에는 반드시 WSAGetLastError의 리턴값이 
//				// ERROR_IO_PENDING이어야 한다. 
//				if (SOCKET_ERROR == nRet && (ERROR_IO_PENDING != WSAGetLastError()))
//				{
//					printf("WSASend: %d\n", WSAGetLastError());
//					CloseClient(lpPerSocketContext);
//				}
//			}
//			else // 데이터가 전부 전송된 경우 
//			{
//				// 다시 이 소켓으로부터 데이터를 받기 위해 WSARecv를 호출한다. 
//				lpIOContext->IOOperation = ClientIoRead;
//				dwRecvNumBytes = 0;
//				dwFlags = 0;
//				buffRecv.buf = lpIOContext->Buffer; // 수신버퍼 지정 
//													// 읽어들일 데이터 크기 지정. 사실 이 크기만큼 데이터를 읽어들여야 
//													// 그 결과가 IOCP큐에 들어가는 것은 아니다.  이 크기 이상 안 
//													// 읽어들일 뿐이고 데이터가 이용가능한 만큼 IOCP큐에 넣는다. 
//				buffRecv.len = MAX_BUFF_SIZE;
//				nRet = WSARecv(lpPerSocketContext->Socket,
//					&buffRecv, 1, &dwRecvNumBytes,
//					&dwFlags, &(lpIOContext->Overlapped), NULL);
//				// SOCKET_ERROR가 리턴된 경우에는 반드시 WSAGetLastError의 리턴값이 
//				// ERROR_IO_PENDING이어야 한다. 
//				if (SOCKET_ERROR == nRet && (ERROR_IO_PENDING != WSAGetLastError()))
//				{
//					printf("WSARecv: %d\n", WSAGetLastError());
//					CloseClient(lpPerSocketContext);
//				}
//			}
//			break;
//		} //switch 
//	} //while 
//	return(0);
//}
