
//Define _WINSOCK_DEPRECATED_NO_WARNINGS to use some function 
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include <time.h>
#include "myfileopt.h"
#include "mynetopt.h"
#include "mydefine.h"

#pragma comment(lib, "Ws2_32.lib")

#define MAX_THREAD 100
#define SEND 20
#define RECV 10

//Define section structure
typedef struct Section {
	WSAOVERLAPPED overlap;
	DWORD recvBytes;
	DWORD sendBytes;
	DWORD toSendBytes;
	DWORD toRecvBytes;
	char buff[BUFF_SIZE];
	WSABUF dataBuff;
	char operation;
	char action;
	char status;
	char fileName[FILE_PATCH_SIZE];
	long long nLeft;
	int ofset;
	unsigned char key;
} Section, *pSection;

//Struc contain socket- SocketStruct
typedef struct SockS {
	SOCKET sock;
} SockS, *pSockS;

//Global folder crictical section
CRITICAL_SECTION folderCrt;

unsigned __stdcall workerThread(void *parma);
int getRandomFileName(char *fileName, int size);
int encodeBlock(char* buff, int buffSize, unsigned char key);
int decodeBlock(char* buff, int buffSize, unsigned char key);
void resetSection(pSection pSect);
int processEncodeMSG(pSection pSect);
int processDecodeMSG(pSection pSect);
int processSendFile(pSection pSect);
int processRecvFile(pSection pSect);
int processRecvMsg(pSection pSect);
int checkArgs(int argc, char ** argv, int *port);
void initSection(pSection pSect);
void closeSection(pSection pSect, pSockS pSock);
void resetToRecv(pSection pSect);
void resetToSend(pSection pSect);

//Main fuction
int main(int argc, char **argv){

	/* Setup argument and winsock*/
	int port;
	if (checkArgs(argc, argv, &port) == 1) return 1;

	//Init winsock v 2.2
	WSAData wsaData;
	int ret;
	ret = WSAStartup(
		MAKEWORD(2, 2),
		&wsaData
	);
	if (ret != 0) {
		printf("Can not init winsock.\n");
		system("pause");
		return 1;
	}

	//Create completion port
	HANDLE completionPort;
	if ((completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)) == NULL) {
		printf("CreateIoCompletionPort() failed with error %d\n", GetLastError());
		return 1;
	}

	//Create threads to communicate and transfer to client
	HANDLE threads[MAX_THREAD];
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	for (int i = 0; i < (int)sysInfo.dwNumberOfProcessors; i++) {
		threads[i] = (HANDLE)_beginthreadex(NULL, 0, workerThread, (void*)completionPort, 0, 0);
		if (threads[i] == 0) {
			printf("Create thread error!\n");
			return 1;
		}
	}

	//Init sever address usse INADDR_ANY argument...
	sockaddr_in severAddr;
	severAddr.sin_family = AF_INET;
	severAddr.sin_addr.s_addr = htonl(ADDR_ANY);
	severAddr.sin_port = htons(port);

	sockaddr_in clientAddr;
	int clientAddrLen = sizeof(clientAddr);

	//Init listen socket use overlaped techinque
	SOCKET listenSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (listenSock == INVALID_SOCKET) {
		printf("Can not init listen socket.\n");
		WSACleanup();
		return 1;
	}

	//Bind address to socket
	ret = bind(listenSock, (const sockaddr *)&severAddr, sizeof(severAddr));
	if (ret != 0) {
		printf("Cand not bind address.\n");
		system("pause");
		WSACleanup();
		return 1;
	}

	//Listen request from client
	if (listen(listenSock, 10)) {
		printf("Error! Cannot listen.");
		system("pause");
		return 0;
	}

	//Server started
	printf("Sever started at: [%s:%d]\n",
		inet_ntoa(severAddr.sin_addr),
		ntohs(severAddr.sin_port));
	
	//Init crictical section
	InitializeCriticalSection(&folderCrt);

	SOCKET connSock;
	DWORD flags = 0;
	Section *pNewSect;
	DWORD transBytes;
	pSockS pNewConnSock;

	while (TRUE) {
		//Accept a new client
		connSock = accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
		if (connSock == SOCKET_ERROR) {
			//Accept() failed
			printf("accept() failed with error: %d\n", WSAGetLastError());
			closesocket(listenSock);
			break;
		}
		else {
			//Accepted a new client
			pNewSect = (pSection)GlobalAlloc(GPTR, sizeof(Section));
			if (pNewSect == NULL) {
				printf("Error to global alloc new section!\n");
				continue;
			}
			//Alloc Socket structure
			pNewConnSock = (SockS *)GlobalAlloc(GPTR, sizeof(SockS));
			if (pNewConnSock == NULL) {
				printf("Error to global alloc new socket structure\n!");
				continue;
			}
			initSection(pNewSect);
			pNewConnSock->sock = connSock;

			printf("Client with socket: %d connected\n", connSock);
			if (CreateIoCompletionPort((HANDLE)connSock, completionPort, (DWORD)pNewConnSock, 0) == NULL) {
				printf("CreateIoCompletionPort() failed with error %d\n", GetLastError());
				break;
			}


			ret = WSARecv(connSock, &pNewSect->dataBuff, 1, &transBytes,
				&flags, &pNewSect->overlap, NULL);
			if (ret == SOCKET_ERROR) {
				if (WSAGetLastError() != WSA_IO_PENDING) {
					printf("Error in connSock\n");
					closeSection(pNewSect, pNewConnSock);
				}
			}
		}
	}

	//Wait for all thread done
	//Never done :))
	WaitForMultipleObjects(sysInfo.dwNumberOfProcessors, threads, TRUE, INFINITE);
	WSACleanup();
	return 0;
}

/*
Worker thread used to recive msg, process it and send respone to client
*/
unsigned __stdcall workerThread(void * parma) {
	HANDLE compPort = (HANDLE)parma;
	pSection pSect;
	pSockS pConnSock;
	DWORD transBytes;
	DWORD flags = 0;
	int recvDone;
	while (TRUE) {
		//Get quence status
		if (GetQueuedCompletionStatus(compPort, &transBytes, (LPDWORD)&pConnSock,
			(LPOVERLAPPED*)&pSect, INFINITE) == 0) {
			printf("Thread have an error in get quence status\n");
			continue;
		}

		//Client disconnect, continue
		if (transBytes == 0) {
			//Connect closed
			closeSection(pSect, pConnSock);
			continue;
		}

		//Check if operator is RECV
		if (pSect->operation == RECV) {
			pSect->recvBytes += transBytes;
			if (pSect->recvBytes < 3) {
				recvDone = 0;
			}
			else {
				pSect->toRecvBytes = *(short*)&pSect->buff[1] + 3;
			}

			if (pSect->toRecvBytes > pSect->recvBytes) {
				//Need to recv
				recvDone = 0;
			}
			else {
				recvDone = 1;
			}

			//Check if recv done
			if (recvDone == 1) {
				pSect->operation = SEND;
				pSect->recvBytes = 0;
				pSect->toRecvBytes = BUFF_SIZE;
				pSect->sendBytes = 0;
				pSect->toSendBytes = processRecvMsg(pSect);
			}
			else {
				resetToRecv(pSect);
				//Call recv function
				if (WSARecv(pConnSock->sock, &pSect->dataBuff, 1, &transBytes,
					&flags, &pSect->overlap, NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != WSA_IO_PENDING) {
						//Error in socket, close section
						closeSection(pSect, pConnSock);
					}
				}
				continue;
			}
		}
		else {
			pSect->sendBytes += transBytes;
		}


		if (pSect->toSendBytes > pSect->sendBytes) {
			//Send is not complete correctly
			resetToSend(pSect);
			if (WSASend(pConnSock->sock, &pSect->dataBuff, 1, &transBytes, flags,
				&pSect->overlap, NULL) == SOCKET_ERROR) {
				if (WSAGetLastError() != WSA_IO_PENDING) {
					//Error in socket, close section
					closeSection(pSect, pConnSock);
					continue;
				}
			}
		}
		else {
			//No more byet to send
			//Recv msg from client
			resetToRecv(pSect);
			if (WSARecv(pConnSock->sock, &pSect->dataBuff, 1, &transBytes,
				&flags, &pSect->overlap, NULL) == SOCKET_ERROR) {
				if (WSAGetLastError() != WSA_IO_PENDING) {
					//Error in socket, close section
					closeSection(pSect, pConnSock);
					continue;
				}
			}
		}
	}
	return 0;
}

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
Function get randome file name in sever
Chose a random name with 4 charecter
IN.OUT char * fileName, contain name of file found
IN int size, size of fileName array
Return length of fileName, 4
*/
int getRandomFileName(char *fileName, int size) {
	int ret;
	srand((unsigned int)time(NULL));
	int i;
	do {
		for (i = 0; i < size - 1; i++) {
			fileName[i] = 'a' + rand() % 26;
		}
		fileName[i] = '\0';
		EnterCriticalSection(&folderCrt);
		ret = fileExist(fileName);
		if (ret == FILE_NOT_EXISTS) {
			ret = createFile(fileName);
		}
		LeaveCriticalSection(&folderCrt);
	} while (ret == FILE_EXISTED);
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
Function process if msg is ENCODE msg
IN pSection pSect: poiter to Section struc contain infomation of section
Return 1, size of code send to client
*/
int processEncodeMSG(pSection pSect) {
	if (pSect->status == UNDEFINED) {
		//Setup to encode
		pSect->status = ENCODE;
		pSect->action = RECV_FILE;
		pSect->key = (unsigned char)pSect->buff[3];
		pSect->ofset = 0;
		//Get randome file name
		getRandomFileName(pSect->fileName, 5);
		pSect->buff[0] = ENCODE;
	}
	else {
		//ERROR scrip
		resetSection(pSect);
		pSect->buff[0] = ERROR_TRANS;
	}
	return 1;
}

/*
Function process if msg is DECODE msg
IN pSection pSect: poiter to Section struc contain infomation of section
Return 1, size of code send to client
*/
int processDecodeMSG(pSection pSect) {
	if (pSect->status == UNDEFINED) {
		//Setup to decode
		pSect->status = DECODE;
		pSect->action = RECV_FILE;
		pSect->key = (unsigned char)pSect->buff[3];
		getRandomFileName(pSect->fileName, 5);
		pSect->ofset = 0;
	}
	else {
		//ERROR Scrip
		resetSection(pSect);
		pSect->buff[0] = ERROR_TRANS;
	}
	return 1;
}

/*
Function process if msg is TRANSFER_FILE
Send a block of file to client
IN pSection pSect: poiter to Section struc contain infomation of section
Return number of bytes send to client
*/
int processSendFile(pSection pSect) {
	if (pSect->status == UNDEFINED) {
		//Error scrip
		//Do some thing here
		pSect->buff[0] = ERROR_TRANS;
		return 1;
	}
	
	short *pLength = (short*)&pSect->buff[1];
	*pLength = 0;
	int ret;

	if (pSect->nLeft == 0) {
		//Send file complete, remove file on sever an reset state
		resetSection(pSect);
		pSect->ofset = 0;
		*pLength = 0;
		//Send to client end of transfering
		pSect->buff[0] = TRANSFER_FILE;
		return 3;
	}
	//Open file
	FILE *f;
	fopen_s(&f, pSect->fileName, "rb");
	if (f == NULL) {
		printf("Error to open file %s\n", pSect->fileName);
		pSect->buff[0] = ERROR_TRANS;
		return 1;
	}

	fseek(f, pSect->ofset, SEEK_SET);

	//Read a block of file to buffer
	ret = fread(&pSect->buff[3], BLOCK_SIZE, 1, f);
	fclose(f);
	if (ret == 1) {
		*pLength = BLOCK_SIZE;
	}
	else {
		if (pSect->nLeft <= BLOCK_SIZE) {
			*pLength = (short)pSect->nLeft;
		}
		else {
			//Error read file
			printf("Can not read file on server\n");
			resetSection(pSect);
			pSect->buff[0] = ERROR_TRANS;
			return 1;
		}
	}
	pSect->nLeft -= *pLength;
	pSect->ofset += *pLength;
	return *pLength + 3;
}

/*
Function process if msg is TRANSFER_FILE msg
Receive a block of file from client
IN pSection pSect: poiter to Section struc contain infomation of section
Return number of byte send to client
*/
int processRecvFile(pSection pSect) {
	if (pSect->status == UNDEFINED) {
		pSect->buff[0] = ERROR_TRANS;
		resetSection(pSect);
		return 1;
	}

	int ret;
	FILE *f;
	short *pLength = (short*)&pSect->buff[1];
	if (*pLength == 0) {
		//Recv file completed, send conplete msg to client
		pSect->action = SEND_FILE;
		pSect->nLeft = getFileSize(pSect->fileName);
		pSect->buff[0] = TRANSFER_FILE;
		pSect->ofset = 0;
		return 3;
	}
	else {
		//Open file to write
		fopen_s(&f, pSect->fileName, "a+b");
		if (f == NULL) {
			printf("Error in open file %s\n", pSect->fileName);
			resetSection(pSect);
			pSect->buff[0] = ERROR_TRANS;
			return 1;
		}

		//Write buffer to file
		if (pSect->status == ENCODE) {
			encodeBlock(&pSect->buff[3], *pLength, pSect->key);
		}
		else {
			decodeBlock(&pSect->buff[3], *pLength, pSect->key);
		}

		ret = fwrite(&pSect->buff[3], *pLength, 1, f);
		fclose(f);
		if (ret == 0) {
			printf("Can not write buffer to file &%s in server\n", pSect->fileName);
			resetSection(pSect);
			pSect->buff[0] = ERROR_TRANS;
			return 1;
		}
	}
	pSect->buff[0] = TRANSFER_FILE;
	return 1;
}

/*
Function process msg from client
Put msg to buffer in Section
IN pSection pSect: poiter to Section struc contain infomation of section
Return number of byte send to client 
*/
int processRecvMsg(pSection pSect) {
	int ret;
	//Receive msg from client
	char opCode = pSect->buff[0];

	//Analaysis msg and process
	switch (opCode) {

	case ENCODE:
		ret = processEncodeMSG(pSect);
		break;

	case DECODE:
		ret = processDecodeMSG(pSect);
		break;

	case TRANSFER_FILE:
		if (pSect->action == RECV_FILE) {
			ret = processRecvFile(pSect);
		}
		if (pSect->action == SEND_FILE) {
			ret = processSendFile(pSect);
		}
		break;

	case ERROR_TRANS:
		resetSection(pSect);
		ret = 0;
		break;
	}
	if (ret == 1) {
		short *pLen =(short*) &pSect->buff[1];
		*pLen = 0;
		ret = 3;
	}
	return ret;
}

/*
Init a section after create it
*/
void initSection(pSection pSect) {
	ZeroMemory(&pSect->overlap, sizeof(OVERLAPPED));
	pSect->dataBuff.buf = pSect->buff;
	pSect->dataBuff.len = BUFF_SIZE;
	pSect->toRecvBytes = BUFF_SIZE;
	pSect->sendBytes = 0;
	pSect->recvBytes = 0;
	pSect->key = 0;
	pSect->toSendBytes = 0;
	pSect->operation = RECV;
	pSect->status = UNDEFINED;
	pSect->ofset = 0;
}

/*
Function reset section in index idx
To status UNDEFINED
And close file pointer
Remove file in sever
*/
void resetSection(pSection pSect) {
	pSect->status = UNDEFINED;
	remove(pSect->fileName);
	//initSection(pSect);
}

//Reset section to send
void resetToSend(pSection pSect) {
	pSect->operation = SEND;
	ZeroMemory(&pSect->overlap, sizeof(OVERLAPPED));
	pSect->recvBytes = 0;
	pSect->toRecvBytes = BUFF_SIZE;
	pSect->dataBuff.buf = pSect->buff + pSect->sendBytes;
	pSect->dataBuff.len = pSect->toSendBytes - pSect->sendBytes;
}

//Reset section to recv;
void resetToRecv(pSection pSect) {
	ZeroMemory(&pSect->overlap, sizeof(OVERLAPPED));
	pSect->operation = RECV;
	pSect->sendBytes = 0;
	pSect->dataBuff.buf = pSect->buff+ pSect->recvBytes;
	pSect->dataBuff.len = pSect->toRecvBytes - pSect->recvBytes;
}

/*
Function close socket 
And free it and the section
*/
void closeSection(pSection pSect, pSockS pSock) {
	if (pSect != NULL && pSock != NULL) {
		//Try to remove file
		remove(pSect->fileName);
		closesocket(pSock->sock);
		GlobalFree(pSect);
		GlobalFree(pSock);
	}
}