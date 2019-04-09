#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

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
#define FILE_PATH_SIZE 256
#define ENCODE 0
#define DECODE  1
#define TRANSFER_FILE 2
#define ERROR_TRANS 3
#define READ_FILE_ERROR -10
#define WRITE_FILE_ERROR -20
#define SEND_SUCCESS 10
#define RECV_SUCCESS 20
#define SCRIPT_ERROR -30
#define ENCODE_CHOSE 4
#define DECODE_CHOSE 5
#define EXIT_CHOSE 6

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
Show options to user
Get option user chose
Return ENCODE_CHOSE if user chose encode option
Return DECODE_CHOSE if user chose decode option
Return EXIT_CHOSE if user chose exit option
*/
int getUserChose() {
	printf("\n");
	char chose[10];
	int done = 0;
	while (1) {
		printf("Chose one of after:\n");
		printf("1: To encode a file\n");
		printf("2: To decode a file\n");
		printf("3: To exit\n");
		printf("Enter 1, 2 or 3 then press \"Enter\": ");
		fflush(stdin);
		while(strlen(gets_s(chose, 10)) < 1);
		if (strcmp(chose, "1") == 0) return ENCODE_CHOSE;
		if (strcmp(chose, "2") == 0) return DECODE_CHOSE;
		if (strcmp(chose, "3") == 0) return EXIT_CHOSE;
		printf("Action not found. Please choose again.\n");
	}
	return 0;
}

/*
Function get file patch an key for encode process
Return 0 if success
*/
int getfilePathAndKey(char * filePath, unsigned char *key) {
	int done = 0;
	do {
		printf("Enter file patch\n");
		fflush(stdin);
		gets_s(filePath, FILE_PATH_SIZE);
		if (fileExist(filePath)==FILE_EXISTED) done = 1;
		else printf("File not exits. Try another file.\n");
	} while (!done);

	done = -1;
	do {
		printf("Enter key [0..255]: ");
		scanf_s("%d", &done);
	} while (done < 0 || done > 255);
	fflush(stdin);
	*key = done;
	return 0;
}

/*
Function check file to decode
File must encoded before decode
Have .enc extend
Retun 0 if found
Else return 1
*/
int checkFileToDecode(char * filePath) {
	return strcmp(&filePath[strlen(filePath) - 4], ".enc");
}

/*
Function trans file name to decode file name
Remove .enc extend
And add -dec to end of file name
Example: filename.mp4.enc -> filename-dec.mp4
*/
void changeFileDecoded(char *filePath) {
	filePath[strlen(filePath) - 4] = '\0';
	int len = strlen(filePath);

	int dotIndex = len-1;
	int i = len;
	while (i >= 0 && filePath[i] != '\\') {
		if (filePath[i] == '.') dotIndex = i;
		i--;
	}
	for (i = len; i >= dotIndex; i--) {
		filePath[i + 4] = filePath[i];
	}
	filePath[dotIndex] = '-';
	filePath[dotIndex + 1] = 'd';
	filePath[dotIndex + 2] = 'e';
	filePath[dotIndex + 3] = 'c';
}

/*
Function get file patch and key for decode
File to decode have to a file is encoded
OUT char *filePath, a string contain file patch
OUT unsigned char *key
*/
void getfilePathAndKeyForDecode(char * filePath, unsigned char *key) {
	int done = 0;
	do {
		printf("Enter file patch\n");
		fflush(stdin);
		gets_s(filePath, FILE_PATH_SIZE);
		if (fileExist(filePath) == FILE_EXISTED && checkFileToDecode(filePath)==0) done = 1;
		else printf("File not exits or file is not encoded. Try another file.\n");
	} while (!done);

	done = -1;
	do {
		printf("Enter key [0..255]: ");
		scanf_s("%d", &done);
	} while (done < 0 || done > 255);
	fflush(stdin);
	*key = done;
}


/*
Function send error code to sever
*/
int sendErrorCodeToSever(SOCKET connSock) {
	char s[2];
	s[0] = ERROR_TRANS;
	return sendExt(connSock, s, 1);
}


/*
Function send file to sever
By block 10kb
Return: SOCKET_ERROR if connection error
Return: READ_FILE_ERROR if read file error
Return SEND_SUCCESS if success
Return ERROR_TRANS if error in tranfer
*/
int sendFileToSever(SOCKET connSock, char *buff, int buffSize, char *filePath) {
	int sendDone = 0;
	int ret;
	buff[0] = (char)TRANSFER_FILE;
	short *pLength = (short*)&buff[1];
	FILE *pFile;
	fopen_s(&pFile, filePath, "rb");
	if (pFile == NULL) {
		sendErrorCodeToSever(connSock);
		return READ_FILE_ERROR;
		
	}
	long long nLeft = getFileSize(filePath);
	fseek(pFile, 0, SEEK_SET);
	if (nLeft <= 0) sendDone = SEND_SUCCESS;
	while (!sendDone && nLeft > 0 && feof(pFile) == 0) {
		buff[0] = TRANSFER_FILE;
		ret = fread(&buff[3], BLOCK_SIZE, 1, pFile);
		if (ret == 0) {
			if (nLeft > BLOCK_SIZE) {
				sendDone = READ_FILE_ERROR;
				sendErrorCodeToSever(connSock);
			}
			else *pLength = (short)nLeft;
		}
		else {
			*pLength = BLOCK_SIZE * ret;
		}
		nLeft -= *pLength;
		if (nLeft <= 0) sendDone = SEND_SUCCESS;
		//Send buff to clien
		ret = sendExt(connSock, buff, *pLength + 3);
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
	//Send trans file success message
	buff[0] = (char)TRANSFER_FILE;
	*pLength = 0;
	ret = sendExt(connSock, buff, 3);
	if (ret == SOCKET_ERROR) {
		return SOCKET_ERROR;
	}

	return sendDone;
}


/*
Function receive result file from sever
By block 10kb
Return: SOCKET_ERROR if connection error
Return: READ_FILE_ERROR if read file error
Return RECV_SUCCESS if success
Return ERROR_TRANS if error in tranfer
*/
int recvFileFromSever(SOCKET connSock, char *buff, int buffSize, char *filePath) {
	int ret;
	char opCode = -1 ;
	FILE *pFile;
	fopen_s(&pFile, filePath, "wb");
	if (pFile == NULL) {
		sendErrorCodeToSever(connSock);
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
			opCode = buff[0];
			if (opCode != TRANSFER_FILE) {
				sendErrorCodeToSever(connSock);
				recvDone = SCRIPT_ERROR;
			}
			else if (*pLength == 0) {
				recvDone = RECV_SUCCESS;
			}
			else {
				ret = fwrite(&buff[3], (int)*pLength, 1, pFile);
				if (ret == 0) {
					sendErrorCodeToSever(connSock);
					recvDone = WRITE_FILE_ERROR;
				}
				//Send to sever
				buff[0] = TRANSFER_FILE;
				ret = sendExt(connSock, buff, 1);
				if (ret == SOCKET_ERROR) {
					recvDone = SOCKET_ERROR;
				}
			}
		}
	}
	fclose(pFile);
	//Remove file if receive not success
	if (recvDone != RECV_SUCCESS) {
		remove(filePath);
	}
	return recvDone;
}

/*
Function process if user chose encode option
Return 0 if have no error
Returnr EXIT_CHOSE if connection error
Return ERROR_TRANS if error in client or sever but not error in connection
*/
int encodeProcess(SOCKET connSock, char *buff, int buffSize) {
	printf("Start encode option\n");
	char filePath[FILE_PATH_SIZE];
	int ret;
	short *pLength = (short*)&buff[1];
	unsigned char key;
	ret = getfilePathAndKey(filePath, &key);
	if (ret != 0) return ret;

	//Send encode code and key to sever
	buff[0] = ENCODE;
	*pLength = 1;
	buff[3] = key;
	ret = sendExt(connSock, buff, 4);
	if (ret == SOCKET_ERROR) return EXIT_CHOSE;

	printf("Send file patch and key to sever: Success\n");
	
	//Send file to sever
	printf("Sending file to sever...\n");

	ret = sendFileToSever(connSock, buff, buffSize, filePath);
	
	if (ret == SOCKET_ERROR) return EXIT_CHOSE;
	
	if (ret != SEND_SUCCESS) return ERROR_TRANS;

	printf("Send file to sever: Success\n");

	//Change file patch
	strcat_s(filePath, ".enc");

	//Receive result from sever
	printf("Receiving result file from sever...\n");
	ret = recvFileFromSever(connSock, buff, buffSize, filePath);

	if (ret == SOCKET_ERROR) return EXIT_CHOSE;
	if (ret != RECV_SUCCESS) return ERROR_TRANS;
	printf("Receive result file from sever: Success\n");
	return 0;
}

/*
Process  if user chose decode option
Return 0 if have no error
Returnr EXIT_CHOSE if connection error
Return ERROR_TRANS if error in client or sever but not error in connection
*/
int decodeProcess(SOCKET connSock, char *buff, int buffSize) {
	printf("Start decode option\n");
	char filePath[FILE_PATH_SIZE];
	int ret;
	short *pLength = (short*)&buff[1];
	unsigned char key;
	getfilePathAndKeyForDecode(filePath, &key);

	//Send encode code and key to sever
	buff[0] = DECODE;
	*pLength = 1;
	buff[3] = key;
	ret = sendExt(connSock, buff, 4);
	if (ret == SOCKET_ERROR) return EXIT_CHOSE;
	printf("Send file patch and key to sever: Success\n");
	//Send file to sever
	printf("Sending file to sever...\n");
	ret = sendFileToSever(connSock, buff, buffSize, filePath);

	if (ret == SOCKET_ERROR) return EXIT_CHOSE;
	
	if (ret != SEND_SUCCESS) return ERROR_TRANS;

	printf("Send file to sever: Success\n");
	//Receive result from sever
	
	//Change file patch
	changeFileDecoded(filePath);

	printf("Receiving result file from sever...\n");
	ret = recvFileFromSever(connSock, buff, buffSize, filePath);
	
	if (ret == SOCKET_ERROR) return EXIT_CHOSE;
	
	if (ret != RECV_SUCCESS) return ERROR_TRANS;
	printf("Receive result file from sever: Success\n");
	return 0;
}

//Main function
int main(int argc, char** argv) {
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
	//setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(int));

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
	int ret;

	char *buff = (char*)malloc(BUFF_SIZE * sizeof(char));
	int chose = 0;
	chose = getUserChose();
	while (chose != EXIT_CHOSE) {
		if (chose == ENCODE_CHOSE) {
			ret = encodeProcess(client, buff, BUFF_SIZE);
		}
		else if (chose == DECODE_CHOSE){
			ret = decodeProcess(client, buff, BUFF_SIZE);
		}
		
		if (ret == EXIT_CHOSE) {
			printf("Error connection.\n");
			chose = EXIT_CHOSE;
			continue;
		}
		else if (ret == ERROR_TRANS) {
			printf("Have errors in transfering. Please try again\n");
		}
		chose = getUserChose();
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