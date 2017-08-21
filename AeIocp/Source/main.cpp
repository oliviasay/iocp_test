#include "stdio.h"
#include "../../AeCommon/AeCommon.h"
#pragma comment(lib, "Ws2_32.lib")

DWORD WINAPI serverThread(LPVOID hIOCP);

DWORD WINAPI processThread(LPVOID hIOCP);

// ����

int main(/*int argc, _TCHAR* argv[]*/)
{
	// Winsock Start - windock.dll �ε�
	WSADATA WSAData;
	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
	{
		printf("Error - Can not load 'winsock.dll' file\n");
		return 1;
	}

	// 1. ���ϻ���  
	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (listenSocket == INVALID_SOCKET)
	{
		printf("Error - Invalid socket\n");
		return 1;
	}

	// �������� ��ü����
	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
	serverAddr.sin_family = PF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	// 2. ���ϼ���
	if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
		printf("Error - Fail bind\n");
		// 6. ��������
		closesocket(listenSocket);
		// Winsock End
		WSACleanup();
		return 1;
	}

	// 3. ���Ŵ�⿭����
	if (listen(listenSocket, 5) == SOCKET_ERROR)
	{
		printf("Error - Fail listen\n");
		// 6. ��������
		closesocket(listenSocket);
		// Winsock End
		WSACleanup();
		return 1;
	}

	// �Ϸ����� ó���ϴ� ��ü(CP : Completion Port) ����
	HANDLE hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	// ��Ŀ������ ����
	// - CPU * 2��
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	int threadCount = systemInfo.dwNumberOfProcessors * 2;
	unsigned long threadId;
	// - thread Handler ����
	HANDLE *hThread = (HANDLE *)malloc(threadCount * sizeof(HANDLE));
	// - thread ����
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

		// ��ø ��Ĺ�� �����ϰ� �Ϸ�� ����� �Լ��� �Ѱ��ش�.
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

	// 6-2. ���� ��������
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
		// ����� �Ϸ� ���
		//LPOVERLAPPED
		//PULONG_PTR  ����ü ���� 8�� 16��
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

//2���Ϸ� 20170817/////////////////////////////////////////////////////////////////////////////////
//int main(/*int argc, _TCHAR* argv[]*/)
//{
//	// Winsock Start - windock.dll �ε�
//	WSADATA WSAData;
//	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
//	{
//		printf("Error - Can not load 'winsock.dll' file\n");
//		return 1;
//	}
//
//	// 1. ���ϻ���  
//	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
//	if (listenSocket == INVALID_SOCKET)
//	{
//		printf("Error - Invalid socket\n");
//		return 1;
//	}
//
//	// �������� ��ü����
//	SOCKADDR_IN serverAddr;
//	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
//	serverAddr.sin_family = PF_INET;
//	serverAddr.sin_port = htons(SERVER_PORT);
//	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
//
//	// 2. ���ϼ���
//	if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
//	{
//		printf("Error - Fail bind\n");
//		// 6. ��������
//		closesocket(listenSocket);
//		// Winsock End
//		WSACleanup();
//		return 1;
//	}
//
//	// 3. ���Ŵ�⿭����
//	if (listen(listenSocket, 5) == SOCKET_ERROR)
//	{
//		printf("Error - Fail listen\n");
//		// 6. ��������
//		closesocket(listenSocket);
//		// Winsock End
//		WSACleanup();
//		return 1;
//	}
//
//	// �Ϸ����� ó���ϴ� ��ü(CP : Completion Port) ����
//	HANDLE hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
//
//	// ��Ŀ������ ����
//	// - CPU * 2��
//	SYSTEM_INFO systemInfo;
//	GetSystemInfo(&systemInfo);
//	int threadCount = systemInfo.dwNumberOfProcessors * 2;
//	unsigned long threadId;
//	// - thread Handler ����
//	HANDLE *hThread = (HANDLE *)malloc(threadCount * sizeof(HANDLE));
//	// - thread ����
//	for (int i = 0; i < threadCount; i++)
//	{
//		hThread[i] = CreateThread(NULL, 0, serverThread, &hIOCP, 0, &threadId);
//	}
//
//	SOCKADDR_IN clientAddr;
//	int addrLen = sizeof(SOCKADDR_IN);
//	memset(&clientAddr, 0, addrLen);
//	SOCKET clientSocket;
//	ClientSession *clientSession = nullptr;
//	DWORD receiveBytes;
//	DWORD flags;
//
//	while (1)
//	{
//		clientSocket = accept(listenSocket, (struct sockaddr *)&clientAddr, &addrLen);
//		if (clientSocket == INVALID_SOCKET)
//		{
//			printf("Error - Accept Failure\n");
//			return 1;
//		}
//
//		clientSession = (struct ClientSession *)malloc(sizeof(struct ClientSession));
//		memset((void *)clientSession, 0x00, sizeof(struct ClientSession));
//		clientSession->socket = clientSocket;
//
//		clientSession->ioContext = (struct IO_CONTEXT *)malloc(sizeof(struct IO_CONTEXT));
//		memset((void *)clientSession->ioContext, 0x00, sizeof(struct IO_CONTEXT));
//
//		memset(&clientSession->ioContext->overlapped,
//			0, sizeof(OVERLAPPED));
//		clientSession->ioContext->receiveBytes = 0;
//		clientSession->ioContext->sendBytes = 0;
//		clientSession->ioContext->dataBuffer.len = MAX_BUFFER;
//		clientSession->ioContext->dataBuffer.buf = clientSession->ioContext->messageBuffer;
//		clientSession->ioContext->IOOperation = IO_OPERATION::ClientIoRead;
//		flags = FLAG_RECV;
//
//		hIOCP = CreateIoCompletionPort((HANDLE)clientSocket, hIOCP, (ULONG_PTR)clientSession, 0);
//
//		// ��ø ��Ĺ�� �����ϰ� �Ϸ�� ����� �Լ��� �Ѱ��ش�.
//		if (WSARecv(clientSession->socket,
//			&clientSession->ioContext->dataBuffer,
//			1,
//			&receiveBytes,
//			&flags,
//			&(clientSession->ioContext->overlapped),
//			NULL))
//		{
//			if (WSAGetLastError() != WSA_IO_PENDING)
//			{
//				printf("Error - IO pending Failure\n");
//				return 1;
//			}
//		}
//	}
//
//	// 6-2. ���� ��������
//	closesocket(listenSocket);
//
//	// Winsock End
//	WSACleanup();
//
//	return 0;
//}
//
//DWORD WINAPI serverThread(LPVOID hIOCP)
//{
//	HANDLE threadHandler = *((HANDLE *)hIOCP);
//	DWORD receiveBytes;
//	DWORD sendBytes;
//	//ClientSession* completionKey = nullptr;
//	LPOVERLAPPED    lpOverlapped = NULL;
//	ClientSession* clientSession = nullptr;
//	DWORD flags;
//	//struct SOCKETINFO *eventSocket;
//	IO_CONTEXT* ioContext = nullptr;
//
//	while (1)
//	{
//		// ����� �Ϸ� ���
//		//LPOVERLAPPED
//		//PULONG_PTR  ����ü ���� 8�� 16��
//		//http://superkwak.blogspot.kr/2009/02/iocp.html
//		if (GetQueuedCompletionStatus(threadHandler,
//			&receiveBytes,
//			(PULONG_PTR)&clientSession,
//			(LPOVERLAPPED *)&lpOverlapped,
//			INFINITE) == 0)
//		{
//			printf("Error - GetQueuedCompletionStatus Failure\n");
//			closesocket(clientSession->socket);
//			free(clientSession);
//			return 1;
//		}
//
//		ioContext = (IO_CONTEXT*)lpOverlapped;
//
//		//clientSession->ioContext = lpOverlapped;
//
//		ioContext->dataBuffer.len = receiveBytes;
//
//		if (receiveBytes == 0)
//		{
//			closesocket(clientSession->socket);
//			free(clientSession);
//			continue;
//		}
//		else
//		{
//			switch (ioContext->IOOperation)
//			{
//			case IO_OPERATION::ClientIoRead:
//			{
//				printf("TRACE - Receive message : %s (%d bytes)\n", ioContext->dataBuffer.buf, ioContext->dataBuffer.len);
//
//				memset(&ioContext->overlapped, 0, sizeof(OVERLAPPED));
//				ioContext->IOOperation = IO_OPERATION::ClientIoWrite;
//				if (WSASend(clientSession->socket,
//					&(ioContext->dataBuffer),
//					1,
//					&sendBytes,
//					0,
//					&(ioContext->overlapped),
//					NULL) == SOCKET_ERROR)
//				{
//					if (WSAGetLastError() != WSA_IO_PENDING)
//					{
//						printf("Error - Fail WSASend(error_code : %d)\n", WSAGetLastError());
//					}
//				}
//				
//				ioContext = (struct IO_CONTEXT *)malloc(sizeof(struct IO_CONTEXT));
//				memset((void *)ioContext, 0x00, sizeof(struct IO_CONTEXT));
//				memset(&ioContext->overlapped, 0, sizeof(OVERLAPPED));
//				memset(ioContext->messageBuffer, 0x00, MAX_BUFFER);
//				ioContext->receiveBytes = 0;
//				ioContext->sendBytes = 0;
//				ioContext->dataBuffer.len = MAX_BUFFER;
//				ioContext->dataBuffer.buf = ioContext->messageBuffer;
//				ioContext->IOOperation = IO_OPERATION::ClientIoRead;
//				flags = FLAG_RECV;
//
//				if (WSARecv(clientSession->socket,
//					&(ioContext->dataBuffer),
//					1,
//					&receiveBytes,
//					&flags,
//					&(ioContext->overlapped),
//					NULL) == SOCKET_ERROR)
//				{
//					if (WSAGetLastError() != WSA_IO_PENDING)
//					{
//						printf("Error - Fail WSARecv(error_code : %d)\n", WSAGetLastError());
//					}
//				}
//
//				//printf("TRACE - Send message : %s (%d bytes)\n", ioContext->dataBuffer.buf, ioContext->dataBuffer.len);
//
//				
//			}break;
//			case IO_OPERATION::ClientIoWrite:
//			{
//				printf("TRACE - Send Complete message : %s (%d bytes)\n", ioContext->dataBuffer.buf, ioContext->dataBuffer.len);
//				//free(ioContext);
//
//				memset(&ioContext->overlapped, 0, sizeof(OVERLAPPED));
//				ioContext->IOOperation = IO_OPERATION::ClientIoWrite;
//				if (WSASend(clientSession->socket,
//					&(ioContext->dataBuffer),
//					1,
//					&sendBytes,
//					0,
//					&(ioContext->overlapped),
//					NULL) == SOCKET_ERROR)
//				{
//					if (WSAGetLastError() != WSA_IO_PENDING)
//					{
//						printf("Error - Fail WSASend(error_code : %d)\n", WSAGetLastError());
//					}
//				}
//
//
//				//memset((void *)ioContext, 0x00, sizeof(struct IO_CONTEXT));
//				//memset(&ioContext->overlapped, 0, sizeof(OVERLAPPED));
//				//memset(ioContext->messageBuffer, 0x00, MAX_BUFFER);
//				//ioContext->receiveBytes = 0;
//				//ioContext->sendBytes = 0;
//				//ioContext->dataBuffer.len = MAX_BUFFER;
//				//ioContext->dataBuffer.buf = ioContext->messageBuffer;
//				//ioContext->IOOperation = IO_OPERATION::ClientIoRead;
//				//flags = FLAG_RECV;
//
//				//if (WSARecv(clientSession->socket,
//				//	&(ioContext->dataBuffer),
//				//	1,
//				//	&receiveBytes,
//				//	&flags,
//				//	&(ioContext->overlapped),
//				//	NULL) == SOCKET_ERROR)
//				//{
//				//	if (WSAGetLastError() != WSA_IO_PENDING)
//				//	{
//				//		printf("Error - Fail WSARecv(error_code : %d)\n", WSAGetLastError());
//				//	}
//				//}
//
//				//printf("TRACE - Send message : %s (%d bytes)\n", ioContext->dataBuffer.buf, ioContext->dataBuffer.len);
//				// �Ϸ�
//			}break;
//			default:
//				break;
//			}			
//		}
//	}
//}



//1���Ϸ�==============================================================================================
//int main(/*int argc, _TCHAR* argv[]*/)
//{
//	// Winsock Start - windock.dll �ε�
//	WSADATA WSAData;
//	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
//	{
//		printf("Error - Can not load 'winsock.dll' file\n");
//		return 1;
//	}
//
//	// 1. ���ϻ���  
//	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
//	if (listenSocket == INVALID_SOCKET)
//	{
//		printf("Error - Invalid socket\n");
//		return 1;
//	}
//
//	// �������� ��ü����
//	SOCKADDR_IN serverAddr;
//	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
//	serverAddr.sin_family = PF_INET;
//	serverAddr.sin_port = htons(SERVER_PORT);
//	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
//
//	// 2. ���ϼ���
//	if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
//	{
//		printf("Error - Fail bind\n");
//		// 6. ��������
//		closesocket(listenSocket);
//		// Winsock End
//		WSACleanup();
//		return 1;
//	}
//
//	// 3. ���Ŵ�⿭����
//	if (listen(listenSocket, 5) == SOCKET_ERROR)
//	{
//		printf("Error - Fail listen\n");
//		// 6. ��������
//		closesocket(listenSocket);
//		// Winsock End
//		WSACleanup();
//		return 1;
//	}
//
//	// �Ϸ����� ó���ϴ� ��ü(CP : Completion Port) ����
//	HANDLE hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
//
//	// ��Ŀ������ ����
//	// - CPU * 2��
//	SYSTEM_INFO systemInfo;
//	GetSystemInfo(&systemInfo);
//	int threadCount = systemInfo.dwNumberOfProcessors * 2;
//	unsigned long threadId;
//	// - thread Handler ����
//	HANDLE *hThread = (HANDLE *)malloc(threadCount * sizeof(HANDLE));
//	// - thread ����
//	for (int i = 0; i < threadCount; i++)
//	{
//		hThread[i] = CreateThread(NULL, 0, serverThread, &hIOCP, 0, &threadId);
//	}
//
//	SOCKADDR_IN clientAddr;
//	int addrLen = sizeof(SOCKADDR_IN);
//	memset(&clientAddr, 0, addrLen);
//	SOCKET clientSocket;
//	SOCKETINFO *socketInfo;
//	DWORD receiveBytes;
//	DWORD flags;
//
//	while (1)
//	{
//		clientSocket = accept(listenSocket, (struct sockaddr *)&clientAddr, &addrLen);
//		if (clientSocket == INVALID_SOCKET)
//		{
//			printf("Error - Accept Failure\n");
//			return 1;
//		}
//
//		socketInfo = (struct SOCKETINFO *)malloc(sizeof(struct SOCKETINFO));
//		memset((void *)socketInfo, 0x00, sizeof(struct SOCKETINFO));
//		socketInfo->socket = clientSocket;
//		socketInfo->receiveBytes = 0;
//		socketInfo->sendBytes = 0;
//		socketInfo->dataBuffer.len = MAX_BUFFER;
//		socketInfo->dataBuffer.buf = socketInfo->messageBuffer;
//		flags = FLAG_RECV;
//
//		hIOCP = CreateIoCompletionPort((HANDLE)clientSocket, hIOCP, (DWORD)socketInfo, 0);
//
//		// ��ø ��Ĺ�� �����ϰ� �Ϸ�� ����� �Լ��� �Ѱ��ش�.
//		if (WSARecv(socketInfo->socket, &socketInfo->dataBuffer, 1, &receiveBytes, &flags, &(socketInfo->overlapped), NULL))
//		{
//			if (WSAGetLastError() != WSA_IO_PENDING)
//			{
//				printf("Error - IO pending Failure\n");
//				return 1;
//			}
//		}
//	}
//
//	// 6-2. ���� ��������
//	closesocket(listenSocket);
//
//	// Winsock End
//	WSACleanup();
//
//	return 0;
//}
//
//class ClientSession
//{
//
//};
//
//DWORD WINAPI serverThread(LPVOID hIOCP)
//{
//	HANDLE threadHandler = *((HANDLE *)hIOCP);
//	DWORD receiveBytes;
//	DWORD sendBytes;
//	ClientSession* completionKey = nullptr;
//	DWORD flags;
//	struct SOCKETINFO *eventSocket;
//	while (1)
//	{
//		// ����� �Ϸ� ���
//		if (GetQueuedCompletionStatus(threadHandler, &receiveBytes, (PULONG_PTR)&completionKey, (LPOVERLAPPED *)&eventSocket, INFINITE) == 0)
//		{
//			printf("Error - GetQueuedCompletionStatus Failure\n");
//			closesocket(eventSocket->socket);
//			free(eventSocket);
//			return 1;
//		}
//
//		eventSocket->dataBuffer.len = receiveBytes;
//
//		if (receiveBytes == 0)
//		{
//			closesocket(eventSocket->socket);
//			free(eventSocket);
//			continue;
//		}
//		else
//		{
//			switch (eventSocket->flags)
//			{
//			case FLAG_RECV:
//			{
//
//			}break;
//			case FLAG_SEND:
//			{
//
//			}break;
//			default:
//				break;
//			}
//
//			printf("TRACE - Receive message : %s (%d bytes)\n", eventSocket->dataBuffer.buf, eventSocket->dataBuffer.len);
//
//			if (WSASend(eventSocket->socket,
//				&(eventSocket->dataBuffer),
//				1,
//				&sendBytes,
//				0,
//				NULL,
//				NULL) == SOCKET_ERROR)
//			{
//				if (WSAGetLastError() != WSA_IO_PENDING)
//				{
//					printf("Error - Fail WSASend(error_code : %d)\n", WSAGetLastError());
//				}
//			}
//
//			printf("TRACE - Send message : %s (%d bytes)\n", eventSocket->dataBuffer.buf, eventSocket->dataBuffer.len);
//
//			memset(eventSocket->messageBuffer, 0x00, MAX_BUFFER);
//			eventSocket->receiveBytes = 0;
//			eventSocket->sendBytes = 0;
//			eventSocket->dataBuffer.len = MAX_BUFFER;
//			eventSocket->dataBuffer.buf = eventSocket->messageBuffer;
//			flags = FLAG_RECV;
//
//			if (WSARecv(eventSocket->socket,
//				&(eventSocket->dataBuffer),
//				1,
//				&receiveBytes,
//				&flags,
//				&eventSocket->overlapped,
//				NULL) == SOCKET_ERROR)
//			{
//				if (WSAGetLastError() != WSA_IO_PENDING)
//				{
//					printf("Error - Fail WSARecv(error_code : %d)\n", WSAGetLastError());
//				}
//			}
//		}
//	}
//}
//===================================================================================================
//DWORD WINAPI makeThread(LPVOID hIOCP)
//{
//	HANDLE threadHandler = *((HANDLE *)hIOCP);
//	DWORD receiveBytes;
//	DWORD sendBytes;
//	ClientSession* completionKey = nullptr;
//	DWORD flags;
//	struct SOCKETINFO *eventSocket;
//	while (1)
//	{
//		// ����� �Ϸ� ���
//		if (GetQueuedCompletionStatus(threadHandler, &receiveBytes, (PULONG_PTR)&completionKey, (LPOVERLAPPED *)&eventSocket, INFINITE) == 0)
//		{
//			printf("Error - GetQueuedCompletionStatus Failure\n");
//			closesocket(eventSocket->socket);
//			free(eventSocket);
//			return 1;
//		}
//
//		eventSocket->dataBuffer.len = receiveBytes;
//
//		if (receiveBytes == 0)
//		{
//			closesocket(eventSocket->socket);
//			free(eventSocket);
//			continue;
//		}
//		else
//		{
//			printf("TRACE - Receive message : %s (%d bytes)\n", eventSocket->dataBuffer.buf, eventSocket->dataBuffer.len);
//
//			//if (WSASend(eventSocket->socket, &(eventSocket->dataBuffer), 1, &sendBytes, 0, NULL, NULL) == SOCKET_ERROR)
//			//{
//			//	if (WSAGetLastError() != WSA_IO_PENDING)
//			//	{
//			//		printf("Error - Fail WSASend(error_code : %d)\n", WSAGetLastError());
//			//	}
//			//}
//
//			printf("TRACE - Send message : %s (%d bytes)\n", eventSocket->dataBuffer.buf, eventSocket->dataBuffer.len);
//
//			memset(eventSocket->messageBuffer, 0x00, MAX_BUFFER);
//			eventSocket->receiveBytes = 0;
//			eventSocket->sendBytes = 0;
//			eventSocket->dataBuffer.len = MAX_BUFFER;
//			eventSocket->dataBuffer.buf = eventSocket->messageBuffer;
//			flags = 0;
//
//			if (WSARecv(eventSocket->socket, &(eventSocket->dataBuffer), 1, &receiveBytes, &flags, &eventSocket->overlapped, NULL) == SOCKET_ERROR)
//			{
//				if (WSAGetLastError() != WSA_IO_PENDING)
//				{
//					printf("Error - Fail WSARecv(error_code : %d)\n", WSAGetLastError());
//				}
//			}
//		}
//	}
//}



	//SYSTEM_INFO si
	//GetSystemInfo(&si);
	//numThreads = si.dwNumberOfProcessors * 2;
	//for (i = 0; i < numThreads; i++)
	//	_beginthreadex(NULL, 0, WorkerThread, ...);


	//while (true)
	//{
	//	DWORD dwTransferred = 0;
	//	OverlappedIOContext* context = nullptr;
	//	ClientSession* asCompletionKey = nullptr;  //Ű�� ������ Ȱ��

	//	int ret = GetQueuedCompletionStatus(
	//		hComletionPort, &dwTransferred,
	//		(PULONG_PTR)&asCompletionKey, (LPOVERLAPPED*)&context, GQCS_TIMEOUT);
	//	DWORD errorCode = GetLastError();

	//	//time outó��
	//	if (ret == 0 && errorCode == WAIT_TIMEOUT)
	//		continue;
	//	//��Ÿ ���� ó��
	//	...

	//		//overlapped ����ü�� I/O Ÿ���� �����ϴ� ���
	//		switch (context->mIoType)
	//		{
	//			//�� I/O�� �����ϴ� �Ϸ��Լ��� �ҷ��� ó���Ѵ�.
	//		case IO_SEND:
	//			completionOk = SendCompletion(asCompletionKey, context, dwTransferred);
	//			break;

	//		case IO_RECV:
	//			completionOk = ReceiveCompletion(asCompletionKey, context, dwTransferred);
	//			break;
	//		}
	//}
//
//	return 0;
//}
//
//bool ReceiveCompletion(const ClientSession* client,
//	OverlappedIOContext* context,
//	DWORD dwTransferred)
//{
//	/// echo back ó�� client->PostSend()���.
//	bool result = true;
//	if (!client->PostSend(context->mBuffer, dwTransferred))
//	{
//		printf_s("PostSend error: %d\n", GetLastError());
//		delete context;
//		return false;
//	}
//
//	delete context;
//	return client->PostRecv();
//}
//
//
//bool ClientSession::PostRecv() const
//{
//	if (!IsConnected())
//		return false;
//
//	//Ŀ���� Overlapped ��ü ���!
//	OverlappedIOContext* recvContext = new OverlappedIOContext(this, IO_RECV);
//
//	DWORD recvbytes = 0;
//	DWORD flags = 0;
//	recvContext->mWsaBuf.buf = recvContext->mBuffer;
//	recvContext->mWsaBuf.len = BUFSIZE;
//
//	//WSARecv�� ���!
//	DWORD ret = WSARecv(
//		mSocket, &(recvContext->mWsaBuf), 1,
//		&recvbytes, &flags,
//		(LPWSAOVERLAPPED)recvContext, NULL);
//
//	//���� ó��
//	return true;
//}