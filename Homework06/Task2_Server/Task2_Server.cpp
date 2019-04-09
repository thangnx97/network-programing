#include <stdio.h>
#include <stdlib.h>

//Define _WINSOCK_DEPRECATED_NO_WARNINGS to use some function 
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include <time.h>
#include "myfileopt.h"
#include "mynetopt.h"

#pragma comment(lib, "Ws2_32.lib")

#define BLOCK_SIZE 10240
#define BUFF_SIZE 10243 // 10kb + 3 byte
#define FILE_PATCH_SIZE 256
#define ENCODE 0
#define DECODE  1
#define TRANSFER_FILE 2
#define ERROR_TRANS 3
#define READ_FILE_ERROR -10
#define WRITE_FILE_ERROR -20
#define SEND_SUCCESS 10
#define RECV_SUCCESS 20
#define SCRIPT_ERROR -30
#define SEND_FILE 11
#define RECV_FILE 12
#define UNDEFINED -10
//Define section struct
typedef struct Section {
	char action;
	char status;
	unsigned char key;
	long long nLeft;
	char fileName[5];
	FILE * pFile;
}Section, *pSection;

Section listSect[FD_SETSIZE];
SOCKET clients[FD_SETSIZE], connSock;
fd_set readfds, initfds;

/*
Function check arguments
Return 0 if all arguments correct
Else retun 1
*/
int checkArgs(int argc, char** argv, int *port) {
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
Function get randome file name in sever
Chose a random name with 4 charecter
IN.OUT char * fileName, contain name of file found
IN int size, size of fileName array
Return length of fileName, 4
*/
int getRandomFileName(char *fileName, int size) {
	srand((unsigned int)time(NULL));
	int i;
	do {
		for (i = 0; i < size - 1; i++) {
			fileName[i] = 'a' + rand() % 26;
		}
		fileName[i] = '\0';
	} while (fileExist(fileName) == FILE_EXISTED);
	return i;
}

/*
Encode a block of data
IN/OUT char *buff, an array of char
*/
int encodeBlock(char* buff, int buffSize, unsigned char key) {
	int i;
	for (i = 0; i < buffSize; i++) {
		buff[i] = (char)(((unsigned char)buff[i] + key) % 256);
	}
	return 0;
}

/*
Decode a block of data
IN/OUT char *buff, an array of char
*/
int decodeBlock(char* buff, int buffSize, unsigned char key) {
	int i;
	for (i = 0; i < buffSize; i++) {
		buff[i] = (char)(((unsigned char)buff[i] - key) % 256);
	}
	return 0;
}

/*
Function use to send error transfer code to client
IN SOCKET connSock, a socket connect to client
Return an value > 0 if SUCCESS
Else return SOCKET_ERROR
*/
int sendErrorCodeToClient(SOCKET connSock) {
	char s[2];
	s[0] = (char)ERROR_TRANS;
	return(sendExt(connSock, s, 1));
}

/*
Function reset section in index idx
To status UNDEFINED
And close file pointer
Remove file in sever
*/
void resetSect(int idx) {
	if (listSect[idx].status != UNDEFINED) {
		listSect[idx].status = UNDEFINED;
		fclose(listSect[idx].pFile);
		remove(listSect[idx].fileName);
	}
}

/*
Function process if msg is ENCODE msg
IN int idx: index in the section array and the client array
IN char *buff: buffer use to transfer
IN int buffSize: size of buffer
Return number of byte send to client or SOCKET_ERROR
*/
char processEncodeMSG(int idx, char *buff, int buffSize) {
	int ret;
	if (listSect[idx].status == UNDEFINED) {
		//Setup to encode
		listSect[idx].status = ENCODE;
		listSect[idx].action = RECV_FILE;
		listSect[idx].key = (unsigned char)buff[3];

		getRandomFileName(listSect[idx].fileName, 5);
		//open file to recv data
		fopen_s(&listSect[idx].pFile, listSect[idx].fileName, "wb");

		if (listSect[idx].pFile != NULL) {
			ret = ENCODE;
		}
		else {
			resetSect(idx);
			ret =ERROR_TRANS;
		}
	}
	else {
		ret = ERROR_TRANS;
	}
	buff[0] = ret;
	return sendExt(clients[idx], buff, 1);
}

/*
Function process if msg is DECODE msg
IN int idx : index in the section array and the client array
IN char *buff : buffer use to transfer
IN int buffSize : size of buffer
Return number of byte send to client or SOCKET_ERROR
*/
char processDecodeMSG(int idx, char *buff, int buffSize) {
	int ret;
	if (listSect[idx].status == UNDEFINED) {
		//Setup to decode
		listSect[idx].status = DECODE;
		listSect[idx].action = RECV_FILE;
		listSect[idx].key = (unsigned char)buff[3];

		getRandomFileName(listSect[idx].fileName, 5);
		//open file to recv data
		fopen_s(&listSect[idx].pFile, listSect[idx].fileName, "wb");

		if (listSect[idx].pFile != NULL) {
			ret = DECODE;
		}
		else {
			resetSect(idx);
			ret = ERROR_TRANS;
		}
	}
	else {
		ret = ERROR_TRANS;
	}
	buff[0] = ret;
	return sendExt(clients[idx], buff, 1);
}

/*
Function process if msg is TRANSFER_FILE
Sen a block of file to client
IN int idx : index in the section array and the client array
IN char *buff : buffer use to transfer
IN int buffSize : size of buffer
Return number of byte send to client or SOCKET_ERROR
*/
char processSendFile(int idx, char *buff, int buffSize) {
	if (listSect[idx].status == UNDEFINED) {
		return sendErrorCodeToClient(clients[idx]);
	}

	short *pLength = (short*)&buff[1];
	int ret;

	if (listSect[idx].nLeft == 0) {
		//Send file complete, remove file on sever an reset state
		resetSect(idx);
		*pLength = 0;
		//Send to client end of transfering
		ret = sendExt(clients[idx], buff, 3);
	}
	else {
		//Read a block of file to buffer
		ret = fread(&buff[3], BLOCK_SIZE, 1, listSect[idx].pFile);
		if (ret == 1) {
			*pLength = BLOCK_SIZE;
		}
		else {
			if (listSect[idx].nLeft <= BLOCK_SIZE) {
				*pLength = (short)listSect[idx].nLeft;
			}
			else {
				//Error read file
				printf("Can not read file on server\n");
				resetSect(idx);
				//Send error code to client
				return sendErrorCodeToClient(clients[idx]);
			}
		}
		listSect[idx].nLeft -= *pLength;
		//Send to client
		ret = sendExt(clients[idx], buff, 3 + *pLength);
	}
	return ret;
}

/*
Function process if msg is TRANSFER_FILE msg
receive a block of file from client
IN int idx : index in the section array and the client array
IN char *buff : buffer use to transfer
IN int buffSize : size of buffer
Return number of byte send to client or SOCKET_ERROR
*/
char processRecvFile(int idx, char *buff, int buffSize) {
	if (listSect[idx].status == UNDEFINED) {
		return sendErrorCodeToClient(clients[idx]);
	}
	int ret;

	short *pLength = (short*)&buff[1];
	if (*pLength == 0) {
		//Recv file completed
		fclose(listSect[idx].pFile);
		listSect[idx].action = SEND_FILE;
		fopen_s(&listSect[idx].pFile, listSect[idx].fileName, "rb");
		if (listSect[idx].pFile == NULL) {
			//Error open file to read
			printf("Can not open file to read on server\n");
			resetSect(idx);
			//Send error code to client
			return sendErrorCodeToClient(clients[idx]);
		}
		//Init to send file
		fseek(listSect[idx].pFile, 0, SEEK_SET);
		listSect[idx].nLeft = getFileSize(listSect[idx].fileName);
		return 1;
	}
	else {
		//Write buffer to file
		if (listSect[idx].status == ENCODE) {
			encodeBlock(&buff[3], *pLength, listSect[idx].key);
		}
		else {
			decodeBlock(&buff[3], *pLength, listSect[idx].key);
		}

		ret = fwrite(&buff[3], *pLength, 1, listSect[idx].pFile);
		if (ret == 0) {
			printf("Can not write buffer to file in server\n");
			fclose(listSect[idx].pFile);
			remove(listSect[idx].fileName);
			listSect[idx].status = UNDEFINED;
			return sendErrorCodeToClient(clients[idx]);
		}
	}
	buff[0] = TRANSFER_FILE;
	return sendExt(clients[idx], buff, 1);
}

/*
Function process with each client
IN int idx : index in the section array and the client array
IN char *buff : buffer use to transfer
IN int buffSize : size of buffer
Return number of byte send to client or SOCKET_ERROR
*/
int processClient(int index, char *buff, int buffSize) {
	
	int ret;
	//Receive msg from client
	ret = recvExt(clients[index], buff, buffSize);
	if (ret <= 0) return ret;
	char opCode = buff[0];
	
	//Analaysis msg and process
	switch (opCode) {

	case ENCODE:
		ret = processEncodeMSG(index, buff, buffSize);
		break;

	case DECODE:
		ret = processDecodeMSG(index, buff, buffSize);
		break;
	
	case TRANSFER_FILE:
		if (listSect[index].action == RECV_FILE) {
			ret = processRecvFile(index, buff, buffSize);
		}
		if (listSect[index].action == SEND_FILE) {
			ret = processSendFile(index, buff, buffSize);
		}
		break;
	
	case ERROR_TRANS:
		if (listSect[index].status != UNDEFINED) {
			resetSect(index);
			ret = 1;
		}
		break;
	}
	return ret;
}

//Main function
int main(int argc, char** argv) {
	int port;
	int ret;
	ret = checkArgs(argc, argv, &port);
	if (ret == 1) {
		return 1;
	}

	//Init winsock v 2.2
	WSAData wsaData;
	ret = WSAStartup(
		MAKEWORD(2, 2),
		&wsaData
	);

	if (ret != 0) {
		printf("Can not init winsock.\n");
		system("pause");
		return 1;
	}

	//Init socket 
	SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSock == SOCKET_ERROR) {
		printf("Can not init socket.\n");
		WSACleanup();
		return 1;
	}

	//Init sever address usse INADDR_ANY argument...
	sockaddr_in severAddr;
	severAddr.sin_family = AF_INET;
	severAddr.sin_addr.s_addr = htonl(ADDR_ANY);
	severAddr.sin_port = htons(port);

	//Bind address to socket
	ret = bind(listenSock, (const sockaddr *)&severAddr, sizeof(severAddr));
	if (ret != 0) {
		printf("Cand not bind address to socket.\n");
		system("pause");
		WSACleanup();
		return 1;
	}

	//Init to transfer
	SOCKET connSock;
	sockaddr_in clientAddr;
	int clientAddrLen = sizeof(clientAddr);
	int nEvents;

	//Listen request
	if (listen(listenSock, 10)) {
		printf("Listen error.\n");
		system("pause");
		WSACleanup();
		return 1;
	}

	//Sever started
	printf("Sever started at: [%s:%d]\n",
		inet_ntoa(severAddr.sin_addr),
		ntohs(severAddr.sin_port));


	//char rcvBuff[BUFF_SIZE], sendBuff[BUFF_SIZE];
	char buff[BUFF_SIZE];

	for (int i = 0; i < FD_SETSIZE; i++) {
		clients[i] = 0;	// 0 indicates available entry
		listSect[i].status = UNDEFINED;
	}

	FD_ZERO(&initfds);
	FD_SET(listenSock, &initfds);

	//Communicate with clients
	while (1) {
		readfds = initfds;
		nEvents = select(0, &readfds, 0, 0, 0);
		if (nEvents < 0) {
			printf("\nError! Cannot poll sockets: %d", WSAGetLastError());
			break;
		}

		//new client connection
		if (FD_ISSET(listenSock, &readfds)) {
			clientAddrLen = sizeof(clientAddr);
			if ((connSock = accept(listenSock, (sockaddr *)&clientAddr, &clientAddrLen)) < 0) {
				printf("\nError! Cannot accept new connection: %d", WSAGetLastError());
				break;
			}
			else {
				//Print client's address
				printf("You got a connection from [%s:%u]\n",
					inet_ntoa(clientAddr.sin_addr),
					ntohs(clientAddr.sin_port));

				int i;
				for (i = 0; i < FD_SETSIZE; i++)
					if (clients[i] == 0) {
						clients[i] = connSock;
						FD_SET(clients[i], &initfds);
						break;
					}

				if (i == FD_SETSIZE) {
					printf("\nToo many clients.");
					closesocket(connSock);
				}

				if (--nEvents == 0)
					continue; //no more event
			}
		}

		//Receive message from clients
		for (int i = 0; i < FD_SETSIZE; i++) {
			if (clients[i] <= 0)
				continue;

			if (FD_ISSET(clients[i], &readfds)) {
				ret = processClient(i, buff, BUFF_SIZE);
				if (ret <= 0) {
					printf("Error: %d\n", WSAGetLastError());
					FD_CLR(clients[i], &initfds);
					listSect[i].status = UNDEFINED;
					closesocket(clients[i]);
					clients[i] = 0;
				}
			}
			if (--nEvents <= 0)
				continue; //no more event
		}
	}

	//End of ussing socket sever
	closesocket(listenSock);
	//End of ussing winsock
	WSACleanup();
	return 0;
}