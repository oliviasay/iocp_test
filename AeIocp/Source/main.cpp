#include "stdio.h"
#include "../../AeCommon/AeCommon.h"
#pragma comment(lib, "Ws2_32.lib")

DWORD WINAPI serverThread(LPVOID hIOCP);

DWORD WINAPI processThread(LPVOID hIOCP);

// 서버

int main(/*int argc, _TCHAR* argv[]*/)
{
	// Winsock Start - windock.dll 로드
	WSADATA WSAData;
	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
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

	// 서버정보 객체설정
	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
	serverAddr.sin_family = PF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	// 2. 소켓설정
	if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
		printf("Error - Fail bind\n");
		// 6. 소켓종료
		closesocket(listenSocket);
		// Winsock End
		WSACleanup();
		return 1;
	}

	// 3. 수신대기열생성
	if (listen(listenSocket, 5) == SOCKET_ERROR)
	{
		printf("Error - Fail listen\n");
		// 6. 소켓종료
		closesocket(listenSocket);
		// Winsock End
		WSACleanup();
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
		hThread[i] = CreateThread(NULL, 0, serverThread, &hIOCP, 0, &threadId);
	}

	SOCKADDR_IN clientAddr;
	int addrLen = sizeof(SOCKADDR_IN);
	memset(&clientAddr, 0, addrLen);
	SOCKET clientSocket;
	ClientSession *clientSession = nullptr;
	DWORD receiveBytes = 0;
	DWORD flags = 0;

	ClientSessionManager* clientSessionManager = new ClientSessionManager();
	while (1)
	{
		clientSocket = accept(listenSocket, (struct sockaddr *)&clientAddr, &addrLen);
		if (clientSocket == INVALID_SOCKET)
		{
			printf("Error - Accept Failure\n");
			return 1;
		}

		ClientSession* clientSession = new ClientSession();
		clientSession->socket = clientSocket;
		clientSession->OnConncted();

		WSABUF dataBuffer;
		dataBuffer.len = MAX_BUFFER;
		dataBuffer.buf = clientSession->ioContext.readBuffer;

		hIOCP = CreateIoCompletionPort((HANDLE)clientSocket, hIOCP, (ULONG_PTR)clientSession, 0);

		// 중첩 소캣을 지정하고 완료시 실행될 함수를 넘겨준다.
		if (WSARecv(clientSession->socket,
			&dataBuffer,
			1,
			&receiveBytes,
			&flags,
			&(clientSession->ioContext.readOverlapped),
			NULL))
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				printf("Error - IO pending Failure\n");
				return 1;
			}
		}
	}

	delete clientSession;
	delete clientSessionManager;

	// 6-2. 리슨 소켓종료
	closesocket(listenSocket);

	// Winsock End
	WSACleanup();

	return 0;
}

DWORD WINAPI serverThread(LPVOID hIOCP)
{
	HANDLE threadHandler = *((HANDLE *)hIOCP);
	DWORD numberOfBytes = 0;
	LPOVERLAPPED    lpOverlapped = NULL;
	ClientSession* clientSession = nullptr;
	OVERLAPPED_EX* overlappedex = nullptr;
	BOOL successed = false;
	DWORD completionKey = 0;

	while (1)
	{
		// 입출력 완료 대기
		//LPOVERLAPPED
		//PULONG_PTR  구조체 변경 8월 16일
		//http://superkwak.blogspot.kr/2009/02/iocp.html
		successed = GetQueuedCompletionStatus(threadHandler,
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

			//printf("TRACE - Send message : %s (%d bytes)\n", ioContext->dataBuffer.buf, ioContext->dataBuffer.len);

		}break;
		case IO_OPERATION::ClientIoWrite:
		{
			printf("TRACE - Send Complete message : (%d bytes)\n", numberOfBytes);
		}break;
		default:
			break;
		}
		
	}
}