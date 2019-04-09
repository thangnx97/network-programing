#include <stdio.h>
#include <stdlib.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <ws2tcpip.h>
#define MAX_PORT 65535 // (2^16) -1
#define BUFF_SIZE 10240 //10Kb
#define FILE_PATCH_SIZE 512
#define FILE_NAME_SIZE 256
#define FILE_EXISTED 1
#define FILE_NOT_EXISTS -1


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
/*Function detect file name from file patch
filePatch: input string contain file patch
fileName: ouput string contain file name
Function return length of file name
*/
int detectFileName(char * filePatch, char * fileName) {
	int i = 0;
	int index = 0;
	while (i < strlen(filePatch)) {
		if (filePatch[i] == '\\') index = i;
		i++;
	}
	for (i = 0; i < strlen(filePatch) - index; i++) {
		fileName[i] = filePatch[index + i + 1];
	}
	fileName[i] = '\0';
	return strlen(fileName);
}

/*Function check file exist
Open file with "r" option

If file exist return 1
Else return 0
*/
int fileExist(char * filePatch) {
	if (strlen(filePatch) == 0) return FILE_NOT_EXISTS;
	FILE *file;
	fopen_s(&file, filePatch, "r");
	if (file == NULL) return FILE_NOT_EXISTS;
	fclose(file);
	return FILE_EXISTED;
}

/*Function get file size
From file patch input
Return file size by byte
*/
_int64 getFileSize(char * filePatch) {
	FILE *file;
	fopen_s(&file, filePatch, "r");
	if (file == NULL) return FILE_NOT_EXISTS;
	_fseeki64(file, 0L, SEEK_END);
	_int64 fileSize = _ftelli64(file);
	fclose(file);
	return fileSize;
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
		detectFileName(filePatch, fileName);
		fileSize = getFileSize(filePatch);
		//Check file exist
		if (fileExist(filePatch) == FILE_EXISTED) {
			//Send file name and file size to sever
			printf("Send to sever: %s", fileName);
			ret = sendExt(client, fileName, strlen(fileName));
			if (ret == SOCKET_ERROR) printf("Transfering is interup\n");
			else {
				//Send file size to sever
				printf("Send to sever: %lld\n", fileSize);
				ret = sendExt(client, (char*)&fileSize, sizeof(fileSize));
				if (ret == SOCKET_ERROR){
					printf("Transfering is interup\n");
				}
				else {
					//Receive respon from sever
					ret = recvExt(client, buff, BUFF_SIZE);
					if (ret == SOCKET_ERROR) printf("Transfering is interup\n");
					else {
						buff[ret] = '\0';
						if (strcmp(buff, "Error: file existed on sever") == 0) {
							puts(buff);
							ret = SOCKET_ERROR;// to avoid while loop
						}
					}
				}
			}

			//Init file to transfer
			FILE *file;
			fopen_s(&file, filePatch, "rb");
			if (file == NULL) {
				printf("Error: Can not access file to read data\n");
				ret = SOCKET_ERROR;
			}
			_int64 nLeft = fileSize;
			if(ret!=SOCKET_ERROR)
				printf("Tranfering file...\n");
			//Loop to trasfer data
			while (ret != SOCKET_ERROR && nLeft != 0) {
				//Send message to sever
				payload = fread(buff, BUFF_SIZE, 1, file);
				ret = sendExt(client, buff, payload);
				if (ret == SOCKET_ERROR) {
					printf("Error: Transfering is interup\n");
				}
				else {
					nLeft -= payload;
					//Receive respon from sever
					ret = recvExt(client, buff, BUFF_SIZE);
					if (ret == SOCKET_ERROR) {
						printf("Error: Transfering is interup\n");
					}
				}
			}
			//Receive respone from sever
			ret = recvExt(client, buff, BUFF_SIZE);
			if (ret == SOCKET_ERROR) {
				printf("Can not receive respone\n");
			}

			fclose(file);
		}
		printf("Enter file patch: ");
		gets_s(filePatch, FILE_PATCH_SIZE);
	}
	//Send end code to sever
	
	//Shutdow tcp socket
	shutdown(client, SD_SEND);
	//End of ussing socket
	closesocket(client);
	//End of ussing winsock
	WSACleanup();
	return 0;
}