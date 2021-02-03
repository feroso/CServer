#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <windows.h>
#include <ws2tcpip.h>
#include <thread>
#include <ctime>

#pragma comment (lib, "Ws2_32.lib")

using namespace std;

int main()
{
	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;	

	struct addrinfo* result = NULL;
	struct addrinfo hints;

	// Read config file
	ifstream inputFileStream("CServer.cfg");
	
	char port[5];
	inputFileStream.getline(port, 5);

	char fileName[20];
	inputFileStream.getline(fileName, 20);

	char ctimeout[5];
	inputFileStream.getline(ctimeout, 5);
	int timeout = atoi(ctimeout);

	cout << "Starting CServer on port: " << port << endl;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, port, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = WSASocket(result->ai_family, result->ai_socktype, result->ai_protocol, NULL, 0, 0);	
	if (ListenSocket == INVALID_SOCKET) {
		printf("WSASocket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	char optval = 1;
	iResult = ::setsockopt(ListenSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
	if (iResult == SOCKET_ERROR) {
		std::cout << 9 << std::endl;
		printf("setsockopt failed with error: %d\n", WSAGetLastError());
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	while (true) {
		// Accept a client socket
		SOCKET * ClientSocket = new SOCKET(INVALID_SOCKET);
		*ClientSocket = accept(ListenSocket, result->ai_addr, (int *)&result->ai_addrlen);

		if (*ClientSocket == INVALID_SOCKET) {
			printf("accept failed with error: %d\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}
		
		// Log ip address
		char * ip = NULL;
		switch (result->ai_addr->sa_family) {
			case AF_INET: {
				struct sockaddr_in* addr_in = (struct sockaddr_in*) result->ai_addr;
				ip = (char *)malloc(INET_ADDRSTRLEN);
				inet_ntop(AF_INET, &(addr_in->sin_addr), ip, INET_ADDRSTRLEN);
				break;
			}
			case AF_INET6: {
				struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*) result->ai_addr;
				ip = (char*)malloc(INET6_ADDRSTRLEN);
				inet_ntop(AF_INET6, &(addr_in6->sin6_addr), ip, INET6_ADDRSTRLEN);
				break;
			}
			default:
				break;
		}
		
		std::time_t current_time = std::time(nullptr);
		char* now = std::asctime(std::localtime(&current_time));
		cout << now;
		cout << " - New incoming connection from: " << ip << endl;
		
		FILE * logFile = fopen("logs.txt", "a");
		fprintf(logFile, "%s - New incoming connection from: %s\n", now, ip);
		fclose(logFile);
		free(ip);

		// Handle incoming connection in a new thread
		std::thread clientThread([ClientSocket, fileName, timeout] {
			STARTUPINFOA si;
			PROCESS_INFORMATION pi;
			
			ZeroMemory(&si, sizeof(si));
			si.cb = sizeof(si);
			si.dwFlags = (STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW);
			si.hStdInput = si.hStdOutput = si.hStdError = (HANDLE)*ClientSocket;
			ZeroMemory(&pi, sizeof(pi));
			
			BOOL succ = CreateProcessA(NULL, (LPSTR)&fileName, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);

			std::this_thread::sleep_for(std::chrono::seconds(timeout));

			succ = TerminateProcess(pi.hProcess, 0);
			CloseHandle(pi.hProcess);

			shutdown(*ClientSocket, SD_SEND);
			closesocket(*ClientSocket);
			free(ClientSocket);
		});

		clientThread.detach();
	}	
}