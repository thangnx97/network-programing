#include <stdio.h>
#include <stdlib.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <conio.h>
#include "mydef.h"
#include "mynetopt.h"


#pragma comment(lib,"Ws2_32.lib")

int checArgs(int argc, char **argv, int *severPort, char *severIp);
int enterPassword(SOCKET sock, char * buff, int buffSize);
int enterUsername(SOCKET sock, char * buff, int buffSize);
int enterPassword(SOCKET sock, char * buff, int buffSize);


int main(int argc, char ** argv) {
	//Process arguments
	int severPort;
	char severIp[16];
	if (checArgs(argc, argv, &severPort, severIp) == 1) {
		return 1;
	}

	//Init winsock
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		printf("Init winsock fail\n");
		system("pause");
		return 1;
	}

	//Init socket
	SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (client == SOCKET_ERROR) {
		printf("Init socket fail\n");
		system("pause");
		return 1;
	}
	printf("Client started!\n");

	//Set receive time out option = 20s
	int tv = 20000;
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(int));

	//Init sever address by input arguments
	sockaddr_in severAddr;
	severAddr.sin_family = AF_INET;
	severAddr.sin_port = htons(severPort);
	severAddr.sin_addr.s_addr = inet_addr(severIp);

	//Request to sever
	printf("Connecting to sever [%s:%d]:\n",
		inet_ntoa(severAddr.sin_addr),
		ntohs(severAddr.sin_port));
	if (connect(client, (const sockaddr*)&severAddr, sizeof(severAddr)) != 0) {
		printf("Can not connect\n");
		system("pause");
		return 1;
	}
	else {
		printf("Connected!\n");
	}

	//Init to transfer
	char buff[BUFF_SIZE];
	int ret, severAddrLen = sizeof(severAddr);
	int disconnect = 0;
	int state = UNDEFINED;

	printf("LOGIN\n");

	do {
		ret = enterUsername(client, buff, BUFF_SIZE);
		if (ret == SOCKET_ERROR) {
			printf("Error code: %d\n", WSAGetLastError());
			disconnect = 1;
		}
		else {
			state = DEFINED;
			ret = enterPassword(client, buff, BUFF_SIZE);
			if (ret == SOCKET_ERROR) {
				printf("Error code: %d\n", WSAGetLastError());
				disconnect = 1;
			}
			else if (ret == ACC_BLOCKED) {
				state = UNDEFINED;
				printf("Account is blocked, login with another account.\n");
			}
			else if (ret == ACC_LOGINED) {
				state = UNDEFINED;
				printf("Account is logined by another client\n");
			}
			else if (ret == PASSWORD_CORRECT) {
				state = LOGINED;
				printf("You are logined.\n");
			}
		}
	} while (state != LOGINED && disconnect == 0);

	//Wait for user chose logout
	if (disconnect == 0) {
		char logout = -1;
		//Wait for user press "Q" to send logout message
		while (logout != 'q' && logout != 'Q') {
			printf("Press \"Q\" to logout\n");
			logout = (char)_getch();
		}
		buff[0] = LOGOUT_MSG;
		ret = sendExt(client, buff, 1);
		if (ret == SOCKET_ERROR) {
			printf("Error code: %d\n", WSAGetLastError());
		}
		else {
			ret = recvExt(client, buff, BUFF_SIZE);
			if (ret == SOCKET_ERROR) {
				printf("Error code: %d\n", WSAGetLastError());
			}
			else {
				//Logout success
				if (buff[0] == LOGOUT_SUCCESS) {
					printf("Logout success.\n");
					disconnect = 1;
				}
			}
		}
	}
	//Disconnect to sever
	printf("Disconnect to sever\n");
	//Shutdow tcp socket
	shutdown(client, SD_SEND);
	//End of ussing socket
	closesocket(client);
	//End of ussing winsock
	WSACleanup();
	system("pause");
	return 0;
}

/*
Function check arguments
If argument correct return 0
Else return 1
*/
int checArgs(int argc, char **argv, int *severPort, char *severIp) {
	if (argc != 3) {
		printf("Command argument not found\n");
		system("pause");
		return 1;
	}
	if (isIpv4Address(argv[1]) == 1) {
		strcpy_s(severIp, 16, argv[1]);
	}
	else {
		printf("Ip not found\n");
		system("pause");
		return 1;
	}

	*severPort = isPortNum(argv[2]);
	if (*severPort == -1) {
		printf("Port input not found\n");
		system("pausse");
		return 1;
	}
	return 0;
}

/*
Function use to enter user name until user name correct
Return USERNAME_CORREC if user name is exist on sever
Else if transfer have error, return SOCKET_ERROE
*/

int enterUsername(SOCKET sock, char * buff, int buffSize) {
	int ret;
	char optCode = -1;
	while (optCode != USERNAME_CORRECT) {
		buff[0] = USERNAME_MSG;
		printf("Enter user name: ");
		gets_s(&buff[1], buffSize - 1);
		ret = sendExt(sock, buff, strlen(buff));
		if (ret == SOCKET_ERROR) {
			printf("Error code: %d\n", WSAGetLastError());
			return SOCKET_ERROR;
		}
		ret = recvExt(sock, buff, buffSize);
		if (ret == SOCKET_ERROR) {
			printf("Error code: %d\n", WSAGetLastError());
			return SOCKET_ERROR;
		}
		optCode = buff[0];
		if (optCode == USERNAME_NOTFOUND) printf("Username not exist\n");
		if (optCode == ACC_BLOCKED) printf("Account is blocked.\
			\nEnter another username:\n");
		if (optCode == ACC_LOGINED) printf("Account is logined by another client.\
			\nEnter another user name:\n");
	}
	//Always return USERNAME_CORRECT
	return optCode;
}

/*
Function use to enter password until password correct, accblocked
or acc is logined by another client
Return PASSWORD_CORRECT if user name is exist on sever
Else return ACC_BLOCED if account is blocked by incorrect more than 3 time
Else if transfer have error, return SOCKET_ERROE
*/
int enterPassword(SOCKET sock, char * buff, int buffSize) {
	int ret;
	char optCode = -1;

	while (optCode != PASSWORD_CORRECT && optCode != ACC_BLOCKED) {
		printf("Enter password: ");
		buff[0] = PASSWORD_MSG;
		gets_s(&buff[1], buffSize - 1);
		ret = sendExt(sock, buff, strlen(buff));
		if (ret == SOCKET_ERROR) {
			printf("Error code: %d\n", WSAGetLastError());
			return SOCKET_ERROR;
		}
		ret = recvExt(sock, buff, buffSize);
		if (ret == SOCKET_ERROR) {
			printf("Error code: %d\n", WSAGetLastError());
			return SOCKET_ERROR;
		}
		optCode = buff[0];
	}
	return optCode;
}
