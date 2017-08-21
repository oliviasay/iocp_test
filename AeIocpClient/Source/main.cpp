/*
## ���� ���� : 1 v n - IOCP
1. socket()            : ���ϻ���
2. connect()        : �����û
3. read()&write()
WIN recv()&send    : ������ �а���
4. close()
WIN closesocket    : ��������
*/

#include "stdio.h"
#include "../../AeCommon/AeCommon.h"
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")

// Ŭ���̾�Ʈ


DWORD WINAPI clientThread(LPVOID hIOCP);

int main(/*int argc, _TCHAR* argv[]*/)
{
	// Winsock Start - winsock.dll �ε�
	WSADATA WSAData;
	if (WSAStartup(MAKEWORD(2, 0), &WSAData) != 0)
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
		hThread[i] = CreateThread(NULL, 0, clientThread, &hIOCP, 0, &threadId);
	}

	ClientSession* clientSession = new ClientSession();
	
	// �������� ��ü����
	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);

	// 2. �����û
	if (connect(listenSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		printf("Error - Fail to connect\n");
		// 4. ��������
		closesocket(listenSocket);
		// Winsock End
		WSACleanup();
		return 1;
	}
	else
	{
		//printf("Server Connected\n* Enter Message\n->");
		printf("Server Connected\n");
	}

	clientSession->socket = listenSocket;
	clientSession->OnConncted();

	hIOCP = CreateIoCompletionPort((HANDLE)listenSocket, hIOCP, (DWORD)clientSession, 0);

	while (1)
	{
		// �޽��� �Է�
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

		// 3-1. ������ ����
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
			//&(clientSession->ioContext.readOverlapped.overlapped),
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
	
	// 4. ��������
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
		// ����� �Ϸ� ���
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
				//&(clientSession->ioContext.sendOverlapped.overlapped),
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
				//&(clientSession->ioContext.readOverlapped.overlapped),
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





////2���Ϸ�/ 20170817////////////////////////////////////////////////////////////////////////////////////////////////
//
///*
//## ���� ���� : 1 v n - IOCP
//1. socket()            : ���ϻ���
//2. connect()        : �����û
//3. read()&write()
//WIN recv()&send    : ������ �а���
//4. close()
//WIN closesocket    : ��������
//*/
//
//#include "stdio.h"
//#include "../../AeCommon/AeCommon.h"
//#include <winsock2.h>
//#pragma comment(lib, "Ws2_32.lib")
//
//// Ŭ���̾�Ʈ
//
//
//DWORD WINAPI clientThread(LPVOID hIOCP);
//
//int main(/*int argc, _TCHAR* argv[]*/)
//{
//	// Winsock Start - winsock.dll �ε�
//	WSADATA WSAData;
//	if (WSAStartup(MAKEWORD(2, 0), &WSAData) != 0)
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
//		hThread[i] = CreateThread(NULL, 0, clientThread, &hIOCP, 0, &threadId);
//	}
//
//	ClientSession *clientSession = nullptr;
//
//	// �������� ��ü����
//	SOCKADDR_IN serverAddr;
//	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
//	serverAddr.sin_family = AF_INET;
//	serverAddr.sin_port = htons(SERVER_PORT);
//	serverAddr.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
//
//	// 2. �����û
//	if (connect(listenSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
//	{
//		printf("Error - Fail to connect\n");
//		// 4. ��������
//		closesocket(listenSocket);
//		// Winsock End
//		WSACleanup();
//		return 1;
//	}
//	else
//	{
//		//printf("Server Connected\n* Enter Message\n->");
//		printf("Server Connected\n");
//	}
//
//	clientSession = (struct ClientSession *)malloc(sizeof(struct ClientSession));
//	memset((void *)clientSession, 0x00, sizeof(struct ClientSession));
//
//	clientSession->ioContext = (struct IO_CONTEXT *)malloc(sizeof(struct IO_CONTEXT));
//	memset((void *)clientSession->ioContext, 0x00, sizeof(struct IO_CONTEXT));
//
//	memset(&clientSession->ioContext->overlapped,0, sizeof(OVERLAPPED));
//
//	clientSession->socket = listenSocket;
//	clientSession->ioContext->receiveBytes = 0;
//	clientSession->ioContext->sendBytes = 0;
//	clientSession->ioContext->dataBuffer.len = MAX_BUFFER;
//	clientSession->ioContext->dataBuffer.buf = clientSession->ioContext->messageBuffer;
//	DWORD flags;
//	DWORD sendBytes = 0;
//	DWORD receiveBytes;
//
//	hIOCP = CreateIoCompletionPort((HANDLE)listenSocket, hIOCP, (DWORD)clientSession, 0);
//
//	while (1)
//	{
//		// �޽��� �Է�
//		char* messageBuffer = clientSession->ioContext->messageBuffer;
//		int i, bufferLen;
//		printf("*Enter Message\n->");
//		for (i = 0; 1; i++)
//		{
//			messageBuffer[i] = getchar();
//			if (messageBuffer[i] == '\n')
//			{
//				messageBuffer[i++] = '\0';
//				break;
//			}
//		}
//		bufferLen = i;
//
//		clientSession->ioContext = (struct IO_CONTEXT *)malloc(sizeof(struct IO_CONTEXT));
//
//		memset((void *)clientSession->ioContext, 0x00, sizeof(struct IO_CONTEXT));
//		memset(&clientSession->ioContext->overlapped, 0, sizeof(OVERLAPPED));
//		clientSession->ioContext->dataBuffer.len = bufferLen;
//		clientSession->ioContext->dataBuffer.buf = messageBuffer;
//		clientSession->ioContext->IOOperation = IO_OPERATION::ClientIoWrite;
//
//		// 3-1. ������ ����
//		if (WSASend(clientSession->socket,
//			&(clientSession->ioContext->dataBuffer),
//			1,
//			&sendBytes,
//			0,
//			&(clientSession->ioContext->overlapped),
//			NULL) == SOCKET_ERROR)
//		{
//			if (WSAGetLastError() != WSA_IO_PENDING)
//			{
//				printf("Error - Fail WSASend(error_code : %d)\n", WSAGetLastError());
//			}
//		}
//
//		//clientSession->ioContext = (struct IO_CONTEXT *)malloc(sizeof(struct IO_CONTEXT));
//		//memset((void *)clientSession->ioContext, 0x00, sizeof(struct IO_CONTEXT));
//
//		//memset(&clientSession->ioContext->overlapped,
//		//	0, sizeof(OVERLAPPED));
//		//memset(clientSession->ioContext->messageBuffer, 0x00, MAX_BUFFER);
//		//clientSession->ioContext->receiveBytes = 0;
//		//clientSession->ioContext->sendBytes = 0;
//		//clientSession->ioContext->dataBuffer.len = MAX_BUFFER;
//		//clientSession->ioContext->dataBuffer.buf = clientSession->ioContext->messageBuffer;
//		//clientSession->ioContext->IOOperation = IO_OPERATION::ClientIoRead;
//		//flags = FLAG_RECV;
//
//		//if (WSARecv(clientSession->socket,
//		//	&clientSession->ioContext->dataBuffer,
//		//	1,
//		//	&receiveBytes,
//		//	&flags,
//		//	&(clientSession->ioContext->overlapped),
//		//	NULL))
//		//{
//		//	if (WSAGetLastError() != WSA_IO_PENDING)
//		//	{
//		//		printf("Error - IO pending Failure\n");
//		//		return 1;
//		//	}
//		//}
//	}
//
//	// 4. ��������
//	closesocket(listenSocket);
//
//	// Winsock End
//	WSACleanup();
//
//	return 0;
//}
//
//
//DWORD WINAPI clientThread(LPVOID hIOCP)
//{
//	HANDLE threadHandler = *((HANDLE *)hIOCP);
//	DWORD receiveBytes;
//	DWORD sendBytes;
//	//ClientSession* completionKey = nullptr;
//	ClientSession* clientSession = nullptr;
//	DWORD flags;
//	LPOVERLAPPED    lpOverlapped = NULL;
//	//struct SOCKETINFO *eventSocket;
//
//	IO_CONTEXT* ioContext = nullptr;
//
//	while (1)
//	{
//		// ����� �Ϸ� ���
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
//		//SOCKET clientSocket = clientSession->socket;
//		ioContext = (IO_CONTEXT*)lpOverlapped;
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
//				//ioContext->IOOperation = IO_OPERATION::ClientIoWrite;
//				//if (WSASend(clientSession->socket,
//				//	&(ioContext->dataBuffer),
//				//	1,
//				//	&sendBytes,
//				//	0,
//				//	&(ioContext->overlapped),
//				//	NULL) == SOCKET_ERROR)
//				//{
//				//	if (WSAGetLastError() != WSA_IO_PENDING)
//				//	{
//				//		printf("Error - Fail WSASend(error_code : %d)\n", WSAGetLastError());
//				//	}
//				//}
//
//
//				//ioContext = (struct IO_CONTEXT *)malloc(sizeof(struct IO_CONTEXT));
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
//			}break;
//			case IO_OPERATION::ClientIoWrite:
//			{
//				printf("TRACE - Send Complete message : %s (%d bytes)\n", ioContext->dataBuffer.buf, ioContext->dataBuffer.len);
//
//				// send�Ҷ� overlapped ����ü ������� �߿���
//				free(ioContext);
//
//				//memset(&ioContext->overlapped, 0, sizeof(OVERLAPPED));
//
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
//
//
//				// �Ϸ�
//			}break;
//			default:
//				break;
//			}
//		}
//	}
//}



//1���Ϸ�///////////////////////////////////////////////////////////////////////////////////////////////////////////
//DWORD WINAPI clientThread(LPVOID hIOCP);
//
//int main(/*int argc, _TCHAR* argv[]*/)
//{
//	// Winsock Start - winsock.dll �ε�
//	WSADATA WSAData;
//	if (WSAStartup(MAKEWORD(2, 0), &WSAData) != 0)
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
//		hThread[i] = CreateThread(NULL, 0, clientThread, &hIOCP, 0, &threadId);
//	}
//
//	SOCKETINFO *clientSession;
//
//	// �������� ��ü����
//	SOCKADDR_IN serverAddr;
//	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
//	serverAddr.sin_family = AF_INET;
//	serverAddr.sin_port = htons(SERVER_PORT);
//	serverAddr.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
//
//	// 2. �����û
//	if (connect(listenSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
//	{
//		printf("Error - Fail to connect\n");
//		// 4. ��������
//		closesocket(listenSocket);
//		// Winsock End
//		WSACleanup();
//		return 1;
//	}
//	else
//	{
//		//printf("Server Connected\n* Enter Message\n->");
//		printf("Server Connected\n");
//	}
//
//	clientSession = (struct SOCKETINFO *)malloc(sizeof(struct SOCKETINFO));
//	memset((void *)clientSession, 0x00, sizeof(struct SOCKETINFO));
//	clientSession->socket = listenSocket;
//	clientSession->receiveBytes = 0;
//	clientSession->sendBytes = 0;
//	clientSession->dataBuffer.len = MAX_BUFFER;
//	clientSession->dataBuffer.buf = clientSession->messageBuffer;
//	DWORD flags;
//	DWORD sendBytes = 0;
//	DWORD receiveBytes;
//
//	hIOCP = CreateIoCompletionPort((HANDLE)listenSocket, hIOCP, (DWORD)clientSession, 0);
//
//	while (1)
//	{
//		// �޽��� �Է�
//		char* messageBuffer = clientSession->messageBuffer;
//		int i, bufferLen;
//		printf("*Enter Message\n->");
//		for (i = 0; 1; i++)
//		{
//			messageBuffer[i] = getchar();
//			if (messageBuffer[i] == '\n')
//			{
//				messageBuffer[i++] = '\0';
//				break;
//			}
//		}
//		bufferLen = i;
//
//		clientSession->dataBuffer.len = bufferLen;
//		clientSession->dataBuffer.buf = messageBuffer;
//
//		// 3-1. ������ ����
//		if (WSASend(clientSession->socket, &(clientSession->dataBuffer), 1, &sendBytes, 0, NULL, NULL) == SOCKET_ERROR)
//		{
//			if (WSAGetLastError() != WSA_IO_PENDING)
//			{
//				printf("Error - Fail WSASend(error_code : %d)\n", WSAGetLastError());
//			}
//		}
//
//		memset(clientSession->messageBuffer, 0x00, MAX_BUFFER);
//		clientSession->receiveBytes = 0;
//		clientSession->sendBytes = 0;
//		clientSession->dataBuffer.len = MAX_BUFFER;
//		clientSession->dataBuffer.buf = clientSession->messageBuffer;
//		flags = FLAG_RECV;
//
//
//		if (WSARecv(clientSession->socket, &clientSession->dataBuffer, 1, &receiveBytes, &flags, &(clientSession->overlapped), NULL))
//		{
//			if (WSAGetLastError() != WSA_IO_PENDING)
//			{
//				printf("Error - IO pending Failure\n");
//				return 1;
//			}
//		}
//	}
//
//	// 4. ��������
//	closesocket(listenSocket);
//
//	// Winsock End
//	WSACleanup();
//
//	return 0;
//}
//
//
//DWORD WINAPI clientThread(LPVOID hIOCP)
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
//			if (WSASend(eventSocket->socket, &(eventSocket->dataBuffer), 1, &sendBytes, 0, NULL, NULL) == SOCKET_ERROR)
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
//
//
















///////////////////////////////////////////////////////////////////////////////////////////////////////////

//struct SOCKETINFO
//{
//	WSAOVERLAPPED overlapped;
//	WSABUF dataBuffer;
//	int receiveBytes;
//	int sendBytes;
//};


//int main(/*int argc, _TCHAR* argv[]*/)
//{
//	// Winsock Start - winsock.dll �ε�
//	WSADATA WSAData;
//	if (WSAStartup(MAKEWORD(2, 0), &WSAData) != 0)
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
//	serverAddr.sin_family = AF_INET;
//	serverAddr.sin_port = htons(SERVER_PORT);
//	serverAddr.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
//
//	// 2. �����û
//	if (connect(listenSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
//	{
//		printf("Error - Fail to connect\n");
//		// 4. ��������
//		closesocket(listenSocket);
//		// Winsock End
//		WSACleanup();
//		return 1;
//	}
//	else
//	{
//		//printf("Server Connected\n* Enter Message\n->");
//		printf("Server Connected\n");
//	}
//
//	SOCKETINFO *socketInfo;
//	DWORD sendBytes;
//	DWORD receiveBytes;
//	DWORD flags;
//
//	while (1)
//	{
//		// �޽��� �Է�
//		char messageBuffer[MAX_BUFFER];
//		int i, bufferLen;
//		printf("*Enter Message\n->");
//		for (i = 0; 1; i++)
//		{
//			messageBuffer[i] = getchar();
//			if (messageBuffer[i] == '\n')
//			{
//				messageBuffer[i++] = '\0';
//				break;
//			}
//		}
//		bufferLen = i;
//
//		socketInfo = (struct SOCKETINFO *)malloc(sizeof(struct SOCKETINFO));
//		memset((void *)socketInfo, 0x00, sizeof(struct SOCKETINFO));
//		socketInfo->dataBuffer.len = bufferLen;
//		socketInfo->dataBuffer.buf = messageBuffer;
//
//		// 3-1. ������ ����
//		int sendBytes = send(listenSocket, messageBuffer, bufferLen, 0);
//		if (sendBytes > 0)
//		{
//			printf("TRACE - Send message : %s (%d bytes)\n", messageBuffer, sendBytes);
//			//// 3-2. ������ �б�
//			//int receiveBytes = recv(listenSocket, messageBuffer, MAX_BUFFER, 0);
//			//if (receiveBytes > 0)
//			//{
//			//	printf("TRACE - Receive message : %s (%d bytes)\n* Enter Message\n->", messageBuffer, receiveBytes);
//			//}
//		}
//
//	}
//
//	// 4. ��������
//	closesocket(listenSocket);
//
//	// Winsock End
//	WSACleanup();
//
//	return 0;
//}
