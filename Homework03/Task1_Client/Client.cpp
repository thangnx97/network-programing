#include <stdio.h>
#include <stdlib.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <ws2tcpip.h>
#define MAX_PORT 65535 // (2^16) -1
#define BUFF_SIZE 256

#define SPLIT_ERROR 1
#define SPLIT_SUCCESS 2
#define FINISH_SPLIT 3

#pragma comment(lib,"Ws2_32.lib")

/*
Function check a string is an ipv4 address
Return 1 if string input is an ipv4 address
Else return 0
*/
int isIpv4Address(char *addr) {
	int len = strlen(addr);
	if (len < 7 || len>15) return 0;
	int dotCount = 0;
	int doubleDot = 1;
	int checkByte = 0;
	int i = 0;
	while (i < len) {
		if (addr[i] == '.') {
			if (doubleDot == 1 || checkByte > 255) return 0;
			doubleDot = 1;
			checkByte = 0;
			dotCount++;
		}
		else {
			if (addr[i]<'0' || addr[i]>'9') return 0;
			doubleDot = 0;
			checkByte = 10 * checkByte + (addr[i] - '0');
		}
		i++;
	}
	if (dotCount > 3 || addr[len] == '.') return 0;
	return 1;
}


/*
Function check if a string is a port number
Return number port if string is a port number
Else return -1
*/
int isPortNum(char *portAddr) {
	int len = strlen(portAddr);
	int i = 0, portNum = 0;
	while (i < len) {
		if (portAddr[i]<'0' || portAddr[i]>'9') return -1;
		portNum = portNum * 10 + (portAddr[i] - '0');
		i++;
	}
	return (portNum <= MAX_PORT) ? portNum : -1;
}

//Function check return value of some function as send, recv
//and show error if ret == SOCKET_ERROR
//Return -1 if ret == SOCKET_ERROR
//Else return 0
int retOption(int ret) {
	if (ret != SOCKET_ERROR) return 0;
	if (WSAGetLastError() == WSAETIMEDOUT) {
		printf("Time out!\n");
	}
	else {
		printf("Error code: %d\n", WSAGetLastError());
	}
	return -1;
}
/*
Receive extend function
Receive data len from sever
After that receive data with dataLen value
Return 1 if receive error
Else return 0
*/
//Function receive extend
//Return...
int recvExt(SOCKET connSock, char * buff, int buffSize) {
	int dataLen;
	int ret;
	//Receive 4 byte of data length
	ret = recv(connSock, (char*)&dataLen, sizeof(int), MSG_WAITALL);
	if (ret == SOCKET_ERROR) return SOCKET_ERROR;
	//Receive data
	if (dataLen < buffSize) {
		ret = recv(connSock, buff, dataLen, MSG_WAITALL);
	}
	return ret;
}

int sendExt(SOCKET connSock, char *buff, int dataLen) {
	int pDataLen = dataLen;
	int ret;
	//Send data length
	ret = send(connSock, (char*)&pDataLen, sizeof(int), 0);
	if (ret != sizeof(int)) return SOCKET_ERROR;
	//Send data
	ret = send(connSock, buff, pDataLen, 0);
	return ret;
}

int main(int argc, char ** argv) {
	//Process arguments
	if (argc != 3) {
		printf("Command argument not found\n");
		system("pause");
		return 1;
	}
	char severIp[16];
	strcpy_s(severIp, argv[1]);
	int severPort;
	severPort = isPortNum(argv[2]);
	if (severPort == -1) {
		printf("Port input not found\n");
		system("pausse");
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

	//Set receive time out option = 10s
	int tv = 10000;
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(int));

	//Init sever address by input arguments
	sockaddr_in severAddr;
	severAddr.sin_family = AF_INET;
	severAddr.sin_port = htons(severPort);
	severAddr.sin_addr.s_addr = inet_addr(severIp);

	//Request to sever
	printf("Connecting to sever [%s:%d]:\n",
		inet_ntoa(severAddr.sin_addr), ntohs(severAddr.sin_port));
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
	char letterStr[BUFF_SIZE], digitStr[BUFF_SIZE];
	//	char transferCode;
	int ret, severAddrLen = sizeof(severAddr);
	printf("Send to sever: ");
	gets_s(buff, BUFF_SIZE);

	//Loop until usser enter empty string
	while (strlen(buff) > 0) {
		//Send message to sever
		ret = sendExt(client, buff, strlen(buff));
		if (ret == SOCKET_ERROR) {
			printf("Error! Can not send message. %d\n", WSAGetLastError());
		}
		else {
			//Receive result message from sever
			ret = recvExt(client, buff, BUFF_SIZE);
			if (retOption(ret) == 0) {
				buff[ret] = '\0';
				if (strcmp(buff, "Error!") == 0) {
					puts(buff);
				}
				else {
					strcpy_s(digitStr, buff);
					ret = recvExt(client, letterStr, BUFF_SIZE);
					if (retOption(ret) == 0) {
						letterStr[ret] = '\0';
						printf("Receive result:\n");
						puts(digitStr);
						puts(letterStr);
					}
				}
			}
		}
		printf("Send to sever: ");
		gets_s(buff, BUFF_SIZE);
	}
	//Send end code to sever
	buff[0] = (char)FINISH_SPLIT;
	ret = sendExt(client, buff, sizeof(char));
	if (ret == SOCKET_ERROR) {
		printf("Error code: %d", WSAGetLastError());
	}
	else {
		if (ret == sizeof(char)) {
			printf("Disconnected.\n");
		}
	}
	//Shutdow tcp socket
	shutdown(client, SD_SEND);
	//End of ussing socket
	closesocket(client);
	//End of ussing winsock
	WSACleanup();
	return 0;
}