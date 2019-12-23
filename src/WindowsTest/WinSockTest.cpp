#define WIN32_LEAN_AND_MEAN

#ifndef _WIN64
#error Don't build in 32bit mode
#endif

#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include "PacketStructure.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "666"

bool established = false;
bool procOpen = false, sentProcRequest = false;

bool RecvPacket(SOCKET socket, ReapRequestGeneric *buffer, int len, int flags)
{
	int iResult;
	ReapErrorReport *error;

	iResult = recv(socket, (char*)buffer, len, flags);

	if (iResult < 0) {
		printf("recv failed with error: %d\n", WSAGetLastError());
		return false;
	}
	else if (iResult == 0) {
		printf("Connection Closed by server.\n");
		return false;
	}
	else if (iResult < sizeof(ReapPacketHeader)) {
		printf("Received a response that was too small to be a pkt.\n");
	}
	else if (strcmp(buffer->magic, "reap") != 0) {
		printf("Received an invalid pkt header :thinking:\n");
	}
	else if (buffer->version != REAP_VERSION) {
		printf("Server version mismatch!\n");
	}
	else {
		switch (buffer-> type) {
		case OperationType_t::PING:
			printf("We Received a ping pkt from the server =)\n");
			established = true;
			break;
		case OperationType_t::OPENPROCESS:
			printf("Server Has our Process ready!\n");
			procOpen = true;
			break;
		case OperationType_t::READPROCESSMEMORY:
			break;
		case OperationType_t::WRITEPROCESSMEMORY:
			break;
		case OperationType_t::ERRORREPORT:
			error = reinterpret_cast<ReapErrorReport*>(buffer);
			printf("ErrorReport: [%s]-(%s)\n", Op2String(error->errorType), error->errorString);
			break;
		default:
			printf("Received unknown response code: (%d)\n", buffer->type);
			break;
		}
	}
	return true;
}
int main()
{
	WSADATA wsaData;
	SOCKET connectSocket = INVALID_SOCKET;
	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;
	char recvbuf[DEFAULT_BUFLEN] = { 0 };
	int iResult;
	int recvbuflen = DEFAULT_BUFLEN;
	
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult) {
		printf("WsaStartup failed with error: %d\n", iResult);
		goto errorout;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	iResult = getaddrinfo("192.168.1.104", "666", &hints, &result);
	if (iResult) {
		printf("getaddrinfo() failed with error: %d\n", iResult);
		WSACleanup();
		goto errorout;
	}

	connectSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (connectSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		goto errorout;
	}

	iResult = connect(connectSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		closesocket(connectSocket);
		WSACleanup();
		printf("ERROR: couldn't connect!\n");
		goto errorout;
	}

	freeaddrinfo(result);

	ReapRequestGeneric request;
	ReapRequestGeneric response;
	ReapOpenProcessRequest *openProc;
	ReapPacketHeader ping;
	ping.type = OperationType_t::PING;

	bool running = true;
	send(connectSocket, reinterpret_cast<const char*>(&ping), sizeof(ReapPacketHeader), 0);
	printf("Sent ping\n");

	while( running ){
		if (!RecvPacket(connectSocket, &response, sizeof(ReapRequestGeneric), 0))
			break; // connection closed by server.

		if (!established)
			continue;

		if (!procOpen && !sentProcRequest) {
			openProc = (ReapOpenProcessRequest*)&request;
			openProc->type = OperationType_t::OPENPROCESS;
			snprintf(openProc->processName, sizeof(openProc->processName), "calculator.exe");
			send(connectSocket, (const char*)openProc, sizeof(ReapOpenProcessRequest), 0);
			printf("sent open proc request.\n");
			sentProcRequest = true;
		}

	}

	closesocket(connectSocket);
	WSACleanup();

	system("pause");
    return 0;


errorout:
	system("pause");
	return 1;
}

