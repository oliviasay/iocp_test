/*
## 소켓 서버 : 1 v n - IOCP
1. socket()            : 소켓생성
2. connect()        : 연결요청
3. read()&write()
WIN recv()&send    : 데이터 읽고쓰기
4. close()
WIN closesocket    : 소켓종료
*/

#include "stdio.h"
#include "../../AeCommon/AeCommon.h"
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")

// 클라이언트


DWORD WINAPI clientThread(LPVOID hIOCP);

int main(/*int argc, _TCHAR* argv[]*/)
{
	// Winsock Start - winsock.dll 로드
	WSADATA WSAData;
	if (WSAStartup(MAKEWORD(2, 0), &WSAData) != 0)
	{
		printf("Error - Can not load 'winsock.dll' file\n");
		return 1;
	}

	// 1. 소켓생성
	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (listenSocket == INVALID_SOCKET)
	{
		printf("Error - Invalid socket\n");
		return 1;
	}

	// 완료결과를 처리하는 객체(CP : Completion Port) 생성
	HANDLE hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	// 워커스레드 생성
	// - CPU * 2개
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	int threadCount = systemInfo.dwNumberOfProcessors * 2;
	unsigned long threadId;
	// - thread Handler 선언
	HANDLE *hThread = (HANDLE *)malloc(threadCount * sizeof(HANDLE));
	// - thread 생성
	for (int i = 0; i < threadCount; i++)
	{
		hThread[i] = CreateThread(NULL, 0, clientThread, &hIOCP, 0, &threadId);
	}

	ClientSession* clientSession = new ClientSession();
	
	// 서버정보 객체설정
	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);

	// 2. 연결요청
	if (connect(listenSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		printf("Error - Fail to connect\n");
		// 4. 소켓종료
		closesocket(listenSocket);
		// Winsock End
		WSACleanup();
		return 1;
	}
	else
	{
		printf("Server Connected\n");
	}

	clientSession->socket = listenSocket;
	clientSession->OnConncted();

	hIOCP = CreateIoCompletionPort((HANDLE)listenSocket, hIOCP, (DWORD)clientSession, 0);

	while (1)
	{
		// 메시지 입력
		char* messageBuffer = clientSession->ioContext.readBuffer;
		int i, bufferLen;
		printf("*Enter Message\n->");
		for (i = 0; 1; i++)
		{
			messageBuffer[i] = getchar();
			if (messageBuffer[i] == '\n')
			{
				messageBuffer[i++] = '\0';
				break;
			}
		}
		bufferLen = i;

		DWORD sendBytes = 0;
		WSABUF dataBuffer;
		dataBuffer.len = bufferLen;
		dataBuffer.buf = clientSession->ioContext.readBuffer;		

		// 3-1. 데이터 쓰기
		if (WSASend(clientSession->socket,
			&(dataBuffer),
			1,
			&sendBytes,
			0,
			&(clientSession->ioContext.sendOverlapped),
			//&(clientSession->ioContext.sendOverlapped.overlapped),
			NULL) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				printf("Error - Fail WSASend(error_code : %d)\n", WSAGetLastError());
			}
		}

		DWORD receiveBytes = 0;
		DWORD flags = 0;

		WSABUF readBuffer;
		readBuffer.len = MAX_BUFFER;
		readBuffer.buf = clientSession->ioContext.readBuffer;

		if (WSARecv(clientSession->socket,
			&(readBuffer),
			1,
			&receiveBytes,
			&flags,
			&(clientSession->ioContext.readOverlapped),
			NULL) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				printf("Error - Fail WSARecv(error_code : %d)\n", WSAGetLastError());
			}
		}
	}

	delete clientSession;
	
	// 4. 소켓종료
	closesocket(listenSocket);

	// Winsock End
	WSACleanup();

	return 0;
}


DWORD WINAPI clientThread(LPVOID hIOCP)
{
	HANDLE threadHandler = *((HANDLE *)hIOCP);
	DWORD numberOfBytes = 0;
	DWORD completionKey = 0;
	ClientSession* clientSession = nullptr;	
	LPOVERLAPPED lpOverlapped = nullptr;
	OVERLAPPED_EX* overlappedex = nullptr;	
	BOOL successed = false;

	while (1)
	{
		// 입출력 완료 대기
		successed = GetQueuedCompletionStatus(
			threadHandler,
			&numberOfBytes,
			(PULONG_PTR)&completionKey,
			(LPOVERLAPPED *)&lpOverlapped,
			INFINITE);

		if (!completionKey) return 0;

		overlappedex = (OVERLAPPED_EX*)lpOverlapped;
		clientSession = overlappedex->clientSession;

		if(!successed ||
			successed && !numberOfBytes)
		{
			printf("Error - GetQueuedCompletionStatus Failure\n");
			OnDisconncted(clientSession);
			continue;
		}

		auto ioType = overlappedex->ioType;
		switch (ioType)
		{
		case IO_OPERATION::ClientIoRead:
		{
			printf("TRACE - Receive message : %s (%d bytes)\n", clientSession->ioContext.readBuffer, numberOfBytes);

			DWORD sendBytes = 0;
			WSABUF wirteBuffer;
			wirteBuffer.len = numberOfBytes;
			wirteBuffer.buf = clientSession->ioContext.readBuffer;

			if (WSASend(clientSession->socket,
				&(wirteBuffer),
				1,
				&sendBytes,
				0,
				&(clientSession->ioContext.sendOverlapped),
				NULL) == SOCKET_ERROR)
			{
				if (WSAGetLastError() != WSA_IO_PENDING)
				{
					printf("Error - Fail WSASend(error_code : %d)\n", WSAGetLastError());
				}
			}

			DWORD receiveBytes = 0;
			DWORD flags = 0;

			WSABUF dataBuffer;
			dataBuffer.len = MAX_BUFFER;
			dataBuffer.buf = clientSession->ioContext.readBuffer;			

			if (WSARecv(clientSession->socket,
				&(dataBuffer),
				1,
				&receiveBytes,
				&flags,
				&(clientSession->ioContext.readOverlapped),
				NULL) == SOCKET_ERROR)
			{
				if (WSAGetLastError() != WSA_IO_PENDING)
				{
					printf("Error - Fail WSARecv(error_code : %d)\n", WSAGetLastError());
				}
			}
		}break;
		case IO_OPERATION::ClientIoWrite:
		{
			printf("TRACE - Send Complete message : (%d bytes)\n", numberOfBytes);
		}break;
		}
	}
}