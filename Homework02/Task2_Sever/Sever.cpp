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
#define BUFF_SIZE 10240 //10Kb
#define FILE_PATCH_SIZE 512
#define FILE_NAME_SIZE 256
#define FILE_EXISTED 1
#define FILE_NOT_EXISTS -1
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


/*Function check file exist
Open file with "r" option

If file exist return 1
Else return 0
*/
int fileExist(char * filePatch) {
	FILE *file;
	fopen_s(&file, filePatch, "r");
	if (file == NULL) return FILE_NOT_EXISTS;
	fclose(file);
	return FILE_EXISTED;
}

//Function receive extend
//Return...
int recvExt(SOCKET connSock, char * buff, int buffSize) {
	int dataLen;
	int ret;
	//Receive 4 byte of data length
	ret = recv(connSock, (char*)&dataLen, sizeof(int), MSG_WAITALL);
	if (ret == SOCKET_ERROR) return SOCKET_ERROR;
	//Receive data
	if (dataLen <= buffSize) {
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
	char fileName[FILE_NAME_SIZE];
	_int64 fileSize;
	_int64 nLeft;
	int clientAddrLen = sizeof(clientAddr);
	int finish;
	int finishConn;
	SOCKET connSock;

	//Listen to connection
	if (listen(sever, 10)) {
		printf("Listen error!\n");
		return 1;
	}
	FILE *file;
	//Infinite lopp
	while (1) {
		//Accept
		connSock = accept(sever, (sockaddr*)&clientAddr, &clientAddrLen);
		printf("Connected to [%s:%u]\n",
			inet_ntoa(clientAddr.sin_addr),
			ntohs(clientAddr.sin_port));
		
		finishConn = 0;
		while (!finishConn) {
			finish = 0;
			file = NULL;
			nLeft = 0;

			//Receiver file name
			ret = recvExt(connSock, fileName, FILE_NAME_SIZE);
			if (ret == SOCKET_ERROR) {
				printf("Can not receive message\n");
				finish = 1;
				finishConn = 1;
			}
			else {
				fileName[ret] = '\0';
				printf("Receive file name: %s\n", fileName);
				//Receive file size
				ret = recvExt(connSock, (char*)&fileSize, sizeof(fileSize));
				if (ret == SOCKET_ERROR) {
					printf("Can not receive file size\n");
				}
				else {
					nLeft = fileSize;
					printf("Receive file size: %lld\n", fileSize);
					//Check file in sever
					if (fileExist(fileName) == FILE_EXISTED) {
						finish = 1;
						strcpy_s(buff, BUFF_SIZE, "Error: file existed on sever");
						//Send to client error
						ret = sendExt(connSock, buff, strlen(buff));
						if (ret == SOCKET_ERROR) {
							printf("Can not send to client\n");
						}
					}
					else {
						fopen_s(&file, fileName, "wb");
						if (file == NULL) {
							printf("Sever can not create file: %s\n", fileName);
							finish = 1;
						}
						else {
							//Transfer file
							while (ret != SOCKET_ERROR && nLeft != 0) {
								//Receive from client
								ret = recvExt(connSock, buff, BUFF_SIZE);
								if (ret == SOCKET_ERROR) {
									printf("Error: Transfering\n");
								}
								else {
									printf("Receive : %d byte\n", ret);
									nLeft -= ret;
									fwrite(buff, ret, 1, file);
									//Send respond to client
									strcpy_s(buff, BUFF_SIZE, "OK");
									ret = sendExt(connSock, buff, strlen(buff));
									/*if (ret == SOCKET_ERROR) {
										printf("Can not send to client\n");
										finish = 1;
									}*/
								}
							}
							//End of transfer file
							fclose(file);
						}
					}
				}
			}
			if (ret != SOCKET_ERROR) {
				//Send to client transfer success
				strcpy_s(buff, BUFF_SIZE, "Successfull transfering");
				ret = sendExt(connSock, buff, strlen(buff));
				if (ret == SOCKET_ERROR)
					printf("Can  not send to client");
			}
			else remove(fileName);
		}
		printf("Disconnected to client\n");
		shutdown(connSock, SD_SEND);
		closesocket(connSock);
	}
	//End of ussing socket sever
	closesocket(sever);
	//End of ussing winsock
	WSACleanup();
	return 0;
}