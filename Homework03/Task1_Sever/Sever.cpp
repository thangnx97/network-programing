#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
Define _WINSOCK_DEPRECATED_NO_WARNINGS to use some function
that not safe in VS2015
*/
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <ws2tcpip.h>
#include <winsock2.h>
#define MAX_PORT 65535 // (2^16) -1
#define BUFF_SIZE 1024
#define FINISH_SPLIT 3
#pragma comment(lib, "Ws2_32.lib")

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

//Split source string to letter string and digit string
//Return 1 if source string contain a charecter is not letter and is not both
//Return 0 if success
int splitString(char* sourceStr, char* letterStr, char* digitStr) {
	int iS = 0,
		iL = 0,
		iD = 0;
	int sourceLen = strlen(sourceStr);
	char chr;
	while (iS < sourceLen) {
		chr = sourceStr[iS];
		if (('a' <= chr && chr <= 'z')||('A'<= chr && chr <='Z')) {
			letterStr[iL++] = chr;
		}
		else if ('0' <= chr && chr <= '9') {
			digitStr[iD++] = chr;
		}
		else {
			letterStr[iL] = '\0';
			digitStr[iD] = '\0';
			return 1;
		}
		iS++;
	}
	letterStr[iL] = '\0';
	digitStr[iD] = '\0';
	if (iD == 0) strcpy_s(digitStr,BUFF_SIZE,  " ");
	if (iL == 0) strcpy_s(letterStr, BUFF_SIZE, " ");
	return 0;
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

//Function receive extend
//Return...
int recvExt(SOCKET connSock, char * buff, int buffSize){
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
	//Pre process argument
	if (argc != 2) {
		printf("Arguments not found\n");
		system("pause");
		return 1;
	}

	int port = isPortNum(argv[1]);
	if (port == -1) {
		printf("Port input not found\n");
		system("pausse");
		return 1;
	}

	/*
	Init winsock
	Use version 2.2
	*/
	WSAData wsaData;
	int ret;
	ret = WSAStartup(
		MAKEWORD(2, 2),
		&wsaData
	);

	if (ret != 0) {
		printf("Canot init winsock!");
		system("pause");
		return 1;
	}

	//Init socket
	SOCKET sever = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sever == SOCKET_ERROR) {
		printf("Error code: %d", WSAGetLastError());
		system("pause");
		return 1;
	}

	//Init sever address usse INADDR_ANY argument...
	sockaddr_in severAddr;
	severAddr.sin_family = AF_INET;
	severAddr.sin_addr.s_addr = htonl(ADDR_ANY);
	severAddr.sin_port = htons(port);

	//Bind address to socket
	ret = bind(sever, (const sockaddr*)&severAddr, sizeof(severAddr));
	if (ret != 0) {
		printf("Can not bind address!\n");
		system("pause");
		return 1;
	}
	printf("Sever started at [%s:%u]\n",
		inet_ntoa(severAddr.sin_addr),
		ntohs(severAddr.sin_port));

	//Init to transfer
	sockaddr_in clientAddr;
	char buff[BUFF_SIZE];
	char letterStr[BUFF_SIZE];
	char digitStr[BUFF_SIZE];
	int clientAddrLen = sizeof(clientAddr);
	int finish;
	SOCKET connSock;

	//Listen to connection
	if (listen(sever, 10)) {
		printf("Listen error!\n");
		return 1;
	}
	printf("Sever started\n");

	//Infinite lopp
	while (1) {
		//Accept a TCP connection
		connSock = accept(sever, (sockaddr*)&clientAddr, &clientAddrLen);
		finish = 0;
		printf("Connected to [%s:%u]\n",
			inet_ntoa(clientAddr.sin_addr),
			ntohs(clientAddr.sin_port));

		//Receiver messange from client
		ret = recvExt(connSock, buff, BUFF_SIZE);
		if (ret == SOCKET_ERROR) {
			printf("Can not receive message\n");
			finish = 1;
		}
		else {
			if (ret == sizeof(char) && buff[0] == FINISH_SPLIT) finish = 1;
			while (!finish) {
				if(0 < ret && ret < BUFF_SIZE) buff[ret] = '\0';
				printf("Recive from client: %s\n", buff);
				ret = splitString(buff, letterStr, digitStr);
				if (ret == 1) {
					//Source string not found
					strcpy_s(buff, "Error!");
					printf("Send to client: %s\n", buff);
					ret = sendExt(connSock, buff, strlen(buff));
					if (ret == SOCKET_ERROR) {
						printf("Error code: %d\n", WSAGetLastError());
					}
				}
				else {
					//Send solution to client
					//Send digitString
					printf("Send digit string to client: %s\n",
						digitStr);
					ret = sendExt(connSock, digitStr, strlen(digitStr));
					if (ret != SOCKET_ERROR) {
						printf("Send letter string to client:%s\n",
							letterStr);
						ret = sendExt(connSock, letterStr, strlen(letterStr));
					}
					if (ret == SOCKET_ERROR) {
						printf("Can not send to sever\n");
					}
				}
				ret = recvExt(connSock, buff, BUFF_SIZE);
				if (ret == SOCKET_ERROR) {
					printf("Can not receive message\n");
					finish = 1;
				}
				//If client send FINISH_SPLIT code finish = 1
				if (ret == sizeof(char) && buff[0] == FINISH_SPLIT) finish = 1;
			}
			//If finish == 1 exit while loop, disconect to client 
			printf("Disconnected to client  [%s:%d]\n",
				inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
			shutdown(connSock, SD_SEND);
			closesocket(connSock);
		}
	}

	//End of ussing socket sever
	closesocket(sever);
	//End of ussing winsock
	WSACleanup();
	return 0;
}