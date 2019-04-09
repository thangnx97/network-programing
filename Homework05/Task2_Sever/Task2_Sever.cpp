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
	srand((unsigned int) time(NULL));
	int i;
	do {
		for (i = 0; i < size - 1; i++) {
			fileName[i] = 'a' + rand()%26;
		}
		fileName[i] = '\0';
	} while (fileExist(fileName)== FILE_EXISTED);
	return i;
}

/*
Encode a block of data
IN/OUT char *buff, an array of char
*/
int encodeBlock(char* buff, int buffSize, unsigned char key) {
	int i;
	for (i = 0; i < buffSize; i++) {
		buff[i] = (char) (((unsigned char) buff[i] + key) % 256);
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
Function receive file and encode it
IN SOCKET connSock : a socket connect to client
IN char *buff: buffer to transfer
IN int buffSize: sizeof buffer
IN unsigned char key: a unsigned integer 8 bit, is key to encode
IN char *fileName: string contain file name to save file from client
Return: RECV_SUCCESS if success
Return: SOCKET_ERROR if connection error
Return: WRITE_FILE_ERROR if error in write file on sever
Return: SCRIPT_ERROR if client send opcode not found
*/
int recvFileAndEncode(SOCKET connSock, char * buff, int buffSize, unsigned char key, char *fileName) {
	int ret;
	char opCode = TRANSFER_FILE;
	FILE *pFile;
	fopen_s(&pFile, fileName, "wb");
	if (pFile == NULL) {
		ret = sendErrorCodeToClient(connSock);
		if (ret == SOCKET_ERROR) return SOCKET_ERROR;
		return WRITE_FILE_ERROR;
	}
	int recvDone = 0;
	short *pLength = (short*)&buff[1];
	while (recvDone == 0) {
		//Receive block of 10kb
		ret = recvExt(connSock, buff, buffSize);
		if (ret == SOCKET_ERROR) {
			recvDone = SOCKET_ERROR;
		}
		else {
			if (opCode != 2) {
				ret = sendErrorCodeToClient(connSock);
				if (ret == SOCKET_ERROR) recvDone = SOCKET_ERROR;
				else recvDone = SCRIPT_ERROR;
			}
			else if (*pLength == 0) {
				recvDone = RECV_SUCCESS;
			}
			else {
				encodeBlock(&buff[3], (int) *pLength, key);
				ret = fwrite(&buff[3], (int)*pLength, 1, pFile);
				if (ret == 0) {
					ret = sendErrorCodeToClient(connSock);
					if (ret == SOCKET_ERROR) recvDone = SOCKET_ERROR;
					else recvDone = WRITE_FILE_ERROR;
				}
				else {
					//Send to client
					buff[0] = TRANSFER_FILE;
					ret = sendExt(connSock, buff, 1);
					if (ret == SOCKET_ERROR) {
						recvDone = SOCKET_ERROR;
					}
				}
			}
		}
	}
	fclose(pFile);
	if (recvDone != RECV_SUCCESS) {
		remove(fileName);
	}
	return recvDone;
}

/*
Function receive file and decode it
IN SOCKET connSock : a socket connect to client
IN char *buff: buffer to transfer
IN int buffSize: sizeof buffer
IN unsigned char key: a unsigned integer 8 bit, is key to decode
IN char *fileName: string contain file name to save file from client
Return: RECV_SUCCESS if success
Return: SOCKET_ERROR if connection error
Return: WRITE_FILE_ERROR if error in write file on sever
Return: SCRIPT_ERROR if client send opcode not found
*/
int recvFileAndDecode(SOCKET connSock, char * buff, int buffSize, unsigned char key, char *fileName) {
	int ret;
	char opCode = TRANSFER_FILE;
	FILE *pFile;
	fopen_s(&pFile, fileName, "wb");
	if (pFile == NULL) {
		ret = sendErrorCodeToClient(connSock);
		if (ret == SOCKET_ERROR) return SOCKET_ERROR;
		return WRITE_FILE_ERROR;
	}
	int recvDone = 0;
	short *pLength = (short*)&buff[1];
	while (recvDone == 0) {
		//Receive block of 10kb
		ret = recvExt(connSock, buff, buffSize);
		if (ret == SOCKET_ERROR) {
			recvDone = SOCKET_ERROR;
		}
		else {
			if (opCode != 2) {
				ret = sendErrorCodeToClient(connSock);
				if (ret == SOCKET_ERROR) recvDone = SOCKET_ERROR;
				else recvDone = SCRIPT_ERROR;
			}
			else if (*pLength == 0) {
				recvDone = RECV_SUCCESS;
			}
			else {
				decodeBlock(&buff[3], (int)*pLength, key);
				ret = fwrite(&buff[3], (int)*pLength, 1, pFile);
				if (ret == 0) {
					ret = sendErrorCodeToClient(connSock);
					if (ret == SOCKET_ERROR) recvDone = SOCKET_ERROR;
					else recvDone = WRITE_FILE_ERROR;
				}
				else {
					//Send to client
					buff[0] = TRANSFER_FILE;
					ret = sendExt(connSock, buff, 1);
					if (ret == SOCKET_ERROR) {
						recvDone = SOCKET_ERROR;
					}
				}
			}
		}
	}
	fclose(pFile);
	if (recvDone != RECV_SUCCESS) {
		remove(fileName);
	}
	return recvDone;
}

/*
Function send file to client
Send error message to client if have error in sesver exept SOCKET_ERROR error
IN SOCKET connSock, a socket connect to client
IN char* buff, buffer to transfer
IN int buffSize, sizeof buffer array
IN char* fileName, name of file will be send in sever
Return SEND_SUCCESS if send file success
Return SOKET_ERROR if connection error
Return READ_FILE_ERROR if error in read file
Return ERROR_TRANS if transfer error
*/
int sendFileToClient(SOCKET connSock, char * buff, int buffSize, char *fileName) {
	int sendDone = 0;
	int ret;
	buff[0] = (char)TRANSFER_FILE;
	short *pLength = (short*)&buff[1];
	FILE *pFile;
	fopen_s(&pFile, fileName, "rb");
	if (pFile == NULL) {
		ret = sendErrorCodeToClient(connSock);
		if (ret == SOCKET_ERROR) return SOCKET_ERROR;
		return READ_FILE_ERROR;
	}
	long long nLeft = getFileSize(fileName);
	fseek(pFile, 0, SEEK_SET);
	if (nLeft <= 0) sendDone = SEND_SUCCESS;
	while (!sendDone && nLeft > 0 && feof(pFile) ==0) {
		ret = fread(&buff[3], BLOCK_SIZE, 1, pFile);
		if (ret == 0) {
			if (nLeft > BLOCK_SIZE) {
				ret = sendErrorCodeToClient(connSock);
				if (ret == SOCKET_ERROR) sendDone = SOCKET_ERROR;
				else sendDone = READ_FILE_ERROR;
			}
			else *pLength  = (short)nLeft;
		}
		else {
			*pLength = BLOCK_SIZE * ret;
		}
		nLeft -= *pLength;
		if (nLeft <= 0) sendDone = SEND_SUCCESS;
		//Send buff to clien
		ret = sendExt(connSock, buff, *pLength+3);
		if (ret == SOCKET_ERROR) {
			sendDone = SOCKET_ERROR;
		}
		else {
			//Receive client respone
			ret = recvExt(connSock, buff, buffSize);
			if (ret == SOCKET_ERROR) {
				sendDone = SOCKET_ERROR;
			}
			else if (buff[0] == ERROR_TRANS) {
				sendDone = ERROR_TRANS;
			}
		}
	}
	//Transfer succes
	fclose(pFile);
	//Send tranfer success msg to client
	buff[0] = TRANSFER_FILE;
	*pLength = 0;
	ret = sendExt(connSock, buff, 3);
	if (ret == SOCKET_ERROR) sendDone = SOCKET_ERROR;
	//Remove file from sever
	remove(fileName);
	return sendDone;
}


/*
Function encode a file from client
IN SOCKET connSock to transfer to client
IN unsigned char * buff is the buffer use to contain massage
IN int BUFF_SIZE size of array buff
IN unsigned char key, the key to encode
Return 0 if encode success
Return ERROR_TRANS if have error in transfer
Return SOCKET_ERROR if connection error
*/
int encode(SOCKET connSock, char * buff, int buffSize, unsigned char key) {
	int done;
	//Resend ENCODE message to client, request to transfer file
	buff[0] = (char)ENCODE;
//	done = sendExt(connSock, buff, 1);
//	if (done == SOCKET_ERROR) return SOCKET_ERROR;

	char fileName[5];
	getRandomFileName(fileName, 5);
	
	//Receive and endcode file
	done = recvFileAndEncode(connSock, buff, buffSize, key, fileName);
	if (done == SOCKET_ERROR) {
		return SOCKET_ERROR;
	}
	if (done != RECV_SUCCESS) {
		//Send error code to client
		return ERROR_TRANS;
	}
	//Send result to client
	done = sendFileToClient(connSock, buff, buffSize, fileName);
	if (done == SOCKET_ERROR) {
		return SOCKET_ERROR;
	}
	if (done != RECV_SUCCESS) {
		//Send error code to client
		return ERROR_TRANS;
	}
	return 0;
}

/*
Function decode a file from client
IN SOCKET connSock to transfer to client
IN unsigned char * buff is the buffer use to contain massage
IN int BUFF_SIZE size of array buff
IN unsigned char key, key to decode
Return 0 if decocde success
Return ERROR_TRANS if have error in transfer
Return SOKET_ERROR if connection error
*/
int decode(SOCKET connSock, char * buff, int buffSize, unsigned char key) {
	int done;
	//Resend ENCODE message to client, request to transfer file
	buff[0] = (char)ENCODE;
//	done = sendExt(connSock, buff, 1);
//	if (done == SOCKET_ERROR) return SOCKET_ERROR;

	char fileName[5];
	getRandomFileName(fileName, 5);

	//Receive and dedcode file
	done = recvFileAndDecode(connSock, buff, buffSize, key, fileName);
	if (done == SOCKET_ERROR) {
		return SOCKET_ERROR;
	}
	if (done != RECV_SUCCESS) {
		return ERROR_TRANS;
	}
	//Send result to client
	done = sendFileToClient(connSock, buff, buffSize, fileName);
	if (done == SOCKET_ERROR) {
		return SOCKET_ERROR;
	}
	if (done != RECV_SUCCESS) {
		return ERROR_TRANS;
	}
	return 0;
}

/*
Thread will start when sever accept a client
IN void* contain connection socket to client
*/
unsigned __stdcall service(void* param) {
	SOCKET connSock = (SOCKET)param;
	char*buff;
	buff = (char*)malloc(BUFF_SIZE * sizeof(char));
	int ret;
	unsigned char key;
	char opCode;
	int disconnect = 0;
	while (disconnect ==0) {
		ret = recvExt(connSock, buff, BUFF_SIZE);
		if (ret == SOCKET_ERROR) {
			disconnect = 1;
		}
		opCode = buff[0];
		if (opCode == ENCODE) {
			key = (unsigned char) buff[3];
			//Encode
			ret = encode(connSock, buff, BUFF_SIZE, key);
			if (ret == SOCKET_ERROR) {
				disconnect = 1;
			}
		}
		else if (opCode == DECODE) {
			//Decode
			key = (unsigned char)buff[3];
			ret = decode(connSock, buff, BUFF_SIZE, key);
			if (ret == SOCKET_ERROR) {
				disconnect = 1;
			}
		}
		else {
			//Access not found
			//Send ERROR massage to client
			ret = sendErrorCodeToClient(connSock);
			if (ret == SOCKET_ERROR) {
				disconnect = 1;
			}
		}
	}
	//Free space of buff array
	free(buff);
	return 0;
}

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
	SOCKET sever = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sever == SOCKET_ERROR) {
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
	ret = bind(sever, (const sockaddr *)&severAddr, sizeof(severAddr));
	if (ret != 0) {
		printf("Cand not bind address to socket.\n");
		system("pause");
		WSACleanup();
		return 1;
	}

	//Sever started
	printf("Sever started at: [%s:%d]\n",
		inet_ntoa(severAddr.sin_addr),
		ntohs(severAddr.sin_port));

	//Init to transfer
	SOCKET connSock;
	sockaddr_in clientAddr;
	int clientAddrLen = sizeof(clientAddr);

	//Listen request
	if (listen(sever, 10)) {
		printf("Listen error.\n");
		system("pause");
		WSACleanup();
		return 1;
	}
	
	//Infinite loop
	while (1) {
		//Acept a connect
		connSock = accept(sever, (sockaddr*)&clientAddr, &clientAddrLen);
		printf("Connect to: [%s:%u]\n",
			inet_ntoa(clientAddr.sin_addr),
			ntohs(clientAddr.sin_port)
		);
		//Create a thread to process client request
		_beginthreadex(0, 0, service, (void *)connSock, 0, 0);
	}

	//End of ussing socket sever
	closesocket(sever);
	//End of ussing winsock
	WSACleanup();
	return 0;
}