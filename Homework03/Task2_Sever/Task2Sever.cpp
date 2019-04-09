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
#include "myfileopt.h"
#include "mynetopt.h"

#define BUFF_SIZE 10240 //10Kb
#define FILE_PATCH_SIZE 512
#define FILE_NAME_SIZE 256
#define FILE_EXIST_ON_SEVER -3
#define TRANSFER_SUCCESS 0
#define FILE_READ_FAIL -4
#define FILE_WRITE_FAIL -5
#define DISCONNECT -10

#pragma comment(lib, "Ws2_32.lib")

/*
Function check arguments
Return 0 if all arguments correct
Else retun 1
*/
int checkArgs(int argc, char ** argv, int *port) {
	//Check argument count. This program argv = 2
	if (argc != 2) {
		printf("Arguments not found\n");
		system("pause");
		return 1;
	}
	//Check argument value
	*port = isPortNum(argv[1]);
	if (*port == -1) {
		printf("Port input not found\n");
		system("pausse");
		return 1;
	}
	return 0;
}

/*
Receive file name and file size from client
Return 0 if sucess
Return SOCKET_ERROR if error transfer
Return FILE_EXIST_ON_SEVER if file existed on sever
*/
int receiveHeader(SOCKET connSock, char* fileName, _int64 *fileSize) {
	int ret;
	char buff[256];
	//Receive file name
	ret = recvExt(connSock, fileName, FILE_NAME_SIZE);
	if (ret == 1 & fileName[0] == DISCONNECT) return SOCKET_ERROR;
	if (ret == SOCKET_ERROR) return SOCKET_ERROR;
	fileName[ret] = '\0';
	printf("Receiver file name: %s\n", fileName);
	//Receiver file size
	ret = recvExt(connSock, (char*)fileSize, sizeof(_int64));
	if (ret == SOCKET_ERROR) return SOCKET_ERROR;
	printf("Receiver file size: %lld byte\n", *fileSize);
	//Check file exist
	if (fileExist(fileName) == FILE_EXISTED) {
		strcpy_s(buff, 256, "Error: file existed on sever");
		ret = sendExt(connSock, buff, strlen(buff));
		if (ret == SOCKET_ERROR) return SOCKET_ERROR;
		return FILE_EXIST_ON_SEVER;
	}
	else {
		strcpy_s(buff, 256, "OK");
		ret = sendExt(connSock, buff, strlen(buff));
		if (ret == SOCKET_ERROR) return SOCKET_ERROR;
	}
	return 0;
}


/*
Function receive file from client
Return TRANSFER_SUCCESS if transfer success
Return TRANS_INTERUP if transfer interup error
Return FILE_WRITE_FAIL if can not write file on sever
*/
int recvData(SOCKET connSock, char * fileName, char * buff, int buffSize, _int64 fileSize) {
	_int64 nLeft = fileSize;
	int ret;
	int payload;
	FILE *pFile;
	fopen_s(&pFile, fileName, "wb");
	if (pFile == NULL) return FILE_WRITE_FAIL;
	while (nLeft>0) {
		//Receive data from client
		ret = recvExt(connSock, buff, buffSize);
		if (ret == SOCKET_ERROR) {
			return SOCKET_ERROR;
			fclose(pFile);
		}
		payload = ret;

		//Write buffer to file
		ret = fwrite(buff, payload, 1, pFile);
		//
		nLeft -= payload;
		//Send to sever
		strcpy_s(buff, buffSize, "OK");
		ret = sendExt(connSock, buff, strlen(buff));
		if (ret == SOCKET_ERROR) {
			fclose(pFile);
			return SOCKET_ERROR;
		}
	}
	//Transfer succes
	fclose(pFile);
	return TRANSFER_SUCCESS;
}



int main(int argc, char ** argv) {
	//Check arguments
	int port;
	if (checkArgs(argc, argv, &port) == 1)
		return 1; //Terminated program
	
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
	int finishConn;
	SOCKET connSock;
	//Listen to connection
	if (listen(sever, 10)) {
		printf("Listen error!\n");
		return 1;
	}
	//Infinite lopp
	while (1) {
		//Accept
		connSock = accept(sever, (sockaddr*)&clientAddr, &clientAddrLen);
		printf("Connected to [%s:%u]\n",
			inet_ntoa(clientAddr.sin_addr),
			ntohs(clientAddr.sin_port));
		
		finishConn = 0;
		while (finishConn ==0) {
			
			//Receiver file name
			ret = receiveHeader(connSock, fileName, &fileSize);
			if (ret == 0) {
				//Start transfer
				printf("Transfering...\n");
				ret = recvData(connSock, fileName, buff, BUFF_SIZE, fileSize);
				if (ret == SOCKET_ERROR) finishConn = 1;
				else if (ret == FILE_WRITE_FAIL) {
					printf("Error: sever write file error\n");
					remove(fileName);
				}
				else if (ret == TRANSFER_SUCCESS) {
					printf("Transfer success\n");
				}
			}
			else {
				if (ret == SOCKET_ERROR) finishConn = 1;
				if (ret == FILE_EXIST_ON_SEVER) printf("File exist on sever\n");
			}
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