#include <stdio.h>
#include <stdlib.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <ws2tcpip.h>
#include "myfileopt.h"
#include "mynetopt.h"

#define BUFF_SIZE 10240 //10Kb
#define FILE_PATCH_SIZE 512
#define FILE_NAME_SIZE 256
#define FILE_EXIST_ON_SEVER -3
#define TRANSFER_SUCCESS 0
#define FILE_READ_FAIL -4
#define DISCONNECT -10
#pragma comment(lib,"Ws2_32.lib")


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
Function send file name and file size to sever
Then receive respone from sever
If respone message is 
Return SOCKET_ERROR if can not send
Return FILE_EXIST_ON_SEVER if file exist on sever
Else return 0 if success
*/
int sendHeader(SOCKET connSock, char * fileName, _int64 fileSize) {
	int ret;
	//Send file name 
	ret = sendExt(connSock, fileName, strlen(fileName));
	if (ret == SOCKET_ERROR) return SOCKET_ERROR;
	//Send file size
	ret = sendExt(connSock, (char*)&fileSize, sizeof(_int64));
	if (ret == SOCKET_ERROR) return SOCKET_ERROR;

	//Receive respone
	char buff[256];
	ret = recvExt(connSock, buff, 256);
	if (ret == SOCKET_ERROR) return SOCKET_ERROR;
	buff[ret] = '\0';
	if (strcmp(buff, "Error: file existed on sever") == 0)
		return FILE_EXIST_ON_SEVER;
	return 0;
}

/*
Function transfer file to sever
Return TRANSFER_SUCCESS if transfer success
Return TRANS_INTERUP if transfer interup error
Return FILE_READ_FAIL if can not read file to transfer
*/
int sendData(SOCKET connSock, char * filePatch, char * buff, int buffSize) {
	_int64 fileSize = getFileSize(filePatch);
	_int64 nLeft = fileSize;
	int ret;
	int payload;
	FILE *pFile;
	fopen_s(&pFile, filePatch, "rb");
	if (pFile == NULL) return FILE_READ_FAIL;
	fseek(pFile, 0L, SEEK_SET);
	while (nLeft > 0 && feof(pFile) == 0) {
		//Read file to buffer
		ret = fread(buff, buffSize, 1, pFile);
		if (ret == 0) {
			if (nLeft > buffSize) {
				return FILE_READ_FAIL;
				fclose(pFile);
			}
			else payload = nLeft;
		}
		else {
			payload = buffSize * ret;
		}
		nLeft -= payload;
		//Send buff to sever
		ret = sendExt(connSock, buff, payload);
		if (ret == SOCKET_ERROR) {
			fclose(pFile);
			return SOCKET_ERROR;
		}
		//Receive sever respone
		ret = recvExt(connSock, buff, buffSize);
		if (ret == SOCKET_ERROR) {
			fclose(pFile);
			return SOCKET_ERROR;
		}
		buff[ret] = '0';
	}
	//Transfer succes
	fclose(pFile);
	return TRANSFER_SUCCESS;
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
	if (isIpv4Address(argv[1])==1) {
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
		inet_ntoa(severAddr.sin_addr),
		ntohs(severAddr.sin_port));
	if (connect(client, (const sockaddr*)&severAddr, sizeof(severAddr)) != 0) {
		printf("Can not connect\n");
		system("pause");
		return 1;
	}
	else {
		//Send echo request 
		//Wait for sever acpect
		printf("Connected!\n");
	}

	//Init to transfer
	char buff[BUFF_SIZE];
	char fileName[FILE_NAME_SIZE];
	char filePatch[FILE_PATCH_SIZE];
	_int64 fileSize;
	
	int payload;
	int ret, severAddrLen = sizeof(severAddr);
	//Loop to user enter corect file patch
	
	printf("Enter file patch: ");
	gets_s(filePatch, FILE_PATCH_SIZE);
	//Loop until user enter empty string
	while (strlen(filePatch) != 0) {
		//Split file name and get file size
		splitFileName(filePatch, fileName);
		fileSize = getFileSize(filePatch);
		
		//Check file exist
		if (fileExist(filePatch) == FILE_EXISTED) {
			//Send file name and file size to sever
			printf("Send to sever\nFile name: %s\nFile size: %lld\n",
				fileName, fileSize);
			ret = sendHeader(client, fileName, fileSize);
			if (ret == 0) {
				//Transfer to sever
				printf("Transfering...\n");
				ret = sendData(client, filePatch, buff, BUFF_SIZE);
				if (ret == SOCKET_ERROR) {
					printf("Error: Transfer interup\n");
				}
				else if (ret == FILE_READ_FAIL) {
					printf("Error: Read file error\n");
				}
				else {
					printf("Successful transfering\n");
				}
			}
			else if(ret == FILE_EXIST_ON_SEVER){
				printf("Error: File existed on sever\n");
			}
			else if (ret == SOCKET_ERROR) {
				printf("Can not send to sever, Error code: %s\n",
					WSAGetLastError());
			}
		}

		if (ret == SOCKET_ERROR) break;
		printf("Enter file patch: ");
		gets_s(filePatch, FILE_PATCH_SIZE);
	}
	//Disconnect to sever
	printf("Disconnect to sever\n");
	buff[0] = DISCONNECT;
	sendExt(client, buff, 1);
	//Shutdow tcp socket
	shutdown(client, SD_SEND);
	//End of ussing socket
	closesocket(client);
	//End of ussing winsock
	WSACleanup();
	return 0;
}