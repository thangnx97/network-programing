#include <stdio.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <string.h>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <process.h>
#include "mynetopt.h"
#pragma comment(lib, "Ws2_32.lib")

#define BUFF_SIZE 128

#define USERNAME_MSG 10
#define PASSWORD_MSG 20
#define LOGOUT_MSG 30

#define USERNAME_NOTFOUND -11
#define USERNAME_CORRECT 11

#define PASSWORD_INCORRECT -21
#define PASSWORD_CORRECT 21

#define LOGOUT_ERROR -31
#define LOGOUT_SUCCESS 31

#define ACC_LOGINED -41
#define ACC_BLOCKED -40
#define ACCESS_DENIED -50
#define ACTIVE '1' //Account is active
#define BLOCKED '0' //Account is blocked
#define USERNAME_SIZE 32
#define PASSWORD_SIZE 32
//Deffine state 
#define UNDEFINED -1
#define DEFINED 0
#define LOGINED 1
#define MAX_SECTIONS 20
#define ACC_FILE "account.txt"
#define MAX_ACC_COUNT 100

//Define section struct
typedef struct Section{
	char countIncoPass;
	char status;
	int indexInListAcc;
} Section, *pSection;

//Define account struct
typedef struct Sccount {
	char username[USERNAME_SIZE];
	char password[PASSWORD_SIZE];
	char status;
} Acc;


/*Global variable*/
SOCKET client[FD_SETSIZE], connSock;
Section listSect[FD_SETSIZE];
fd_set readfds, initfds; //use initfds to initiate readfds at the begining of every loop step
sockaddr_in clientAddr;
Acc *listAcc;
int listLen;

int splitAcc(char *str, Acc* pAcc);
int readFileToList(Acc **list, const char * fileName);
int updateFileAcc(Acc *list, int listLen, char *fileName);
int checkArgs(int argc, char ** argv, int *port);
int getIndexByUsn(char *username);
int processClient(int index, char *buff, int buffSize);
int usernameProcess(int index, char *buff, int buffSize);
int passwordProcess(int index, char *buff, int buffSize);
int logoutProcess(int index, char *buff, int buffSize);

int main(int argc, char **argv) {
	int port;
	if (checkArgs(argc, argv, &port) == 1) return 1;

	//Read data from file to memory
	listLen = readFileToList(&listAcc, ACC_FILE);
	if (listLen <1) {
		if (listLen == -1) printf("Read data from file error.\n");
		else printf("No data correct in file.\n");
		system("pause");
		return 1;
	}

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

	//Sever started
	printf("Sever started at: [%s:%d]\n",
		inet_ntoa(severAddr.sin_addr),
		ntohs(severAddr.sin_port));

	//Setup data
	listLen = readFileToList(&listAcc, ACC_FILE);



	int nEvents, clientAddrLen;
	//char rcvBuff[BUFF_SIZE], sendBuff[BUFF_SIZE];
	char buff[BUFF_SIZE];

	for (int i = 0; i < FD_SETSIZE; i++) {
		client[i] = 0;	// 0 indicates available entry
		listSect[i].status = UNDEFINED;
		listSect[i].countIncoPass = 0;
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
					if (client[i] == 0) {
						client[i] = connSock;
						FD_SET(client[i], &initfds);
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
			if (client[i] <= 0)
				continue;

			if (FD_ISSET(client[i], &readfds)) {
				ret = processClient(i, buff, BUFF_SIZE);
				if (ret <= 0) {
					printf("Error: %d\n", WSAGetLastError());
					FD_CLR(client[i], &initfds);
					listSect[i].status = UNDEFINED;
					listSect[i].countIncoPass = 0;
					closesocket(client[i]);
					client[i] = 0;				
				}
			}
			if (--nEvents <= 0)
				continue; //no more event
		}
	}
	closesocket(listenSock);
	WSACleanup();
	return 0;
}

/*********************************************************/
/*
Function split string contain username, password and status
to an account struct
*/
int splitAcc(char *str, Acc* pAcc) {
	int i = 0;
	int j = 0;
	//Split user name
	while (str[i] != ' ') {
		pAcc->username[j] = str[i];
		i++;
		j++;
	}
	pAcc->username[j] = '\0';
	j = 0;
	i++;
	//Split password
	while (str[i] != ' ') {
		pAcc->password[j] = str[i];
		i++;
		j++;
	}
	pAcc->password[j] = '\0';
	i++;
	pAcc->status = str[i];
	return 0;
}

/*
Function read file contain accout infomation
And save it to a list account
IN a const string of fileName
OUT Acc **list a pointer to list of Account
Return size of list account
*/
int readFileToList(Acc **list, const char * fileName) {
	FILE *file;
	fopen_s(&file, fileName, "r");
	if (file == NULL) return -1;
	*list = (Acc*)malloc(MAX_ACC_COUNT * sizeof(Acc));
	int i = 0;
	char buff[66];
	while (!feof(file)) {
		fgets(buff, 66, file);
		splitAcc(buff, *list + i);
		i++;
	}
	fclose(file);
	return i;
}

/*
Function update account information
from list accoutto file
return 0
*/
int updateFileAcc(Acc *list, int listLen, char *fileName) {
	FILE *file;
	fopen_s(&file, ACC_FILE, "w");
	// if file == NULL
	char buff[100];
	buff[0] = '\0';
	int i, j;
	for (i = 0; i < listLen - 1; i++) {
		strcat_s(buff, list[i].username);
		strcat_s(buff, " ");
		strcat_s(buff, list[i].password);
		j = strlen(buff);
		buff[j] = ' ';
		buff[j + 1] = list[i].status;
		buff[j + 2] = '\n';
		buff[j + 3] = '\0';
		fputs(buff, file);
		buff[0] = '\0';
	}
	buff[0] = '\0';
	strcat_s(buff, list[i].username);
	strcat_s(buff, " ");
	strcat_s(buff, list[i].password);
	j = strlen(buff);
	buff[j] = ' ';
	buff[j + 1] = list[i].status;
	buff[j + 2] = '\0';

	fputs(buff, file);
	fclose(file);
	return 0;
}

/*
Function find index of account with username
IN: username
Return: USERNAME_NOTFOUND if can not find user name in account list
esle return ACC_BLOCKED if account is blocked
else return index of account with this user name
*/
int getIndexByUsn(char *username) {
	int i;
	int index = -1;
	for (i = 0; i < listLen; i++) {
		if (strcmp(username, listAcc[i].username) == 0) {
			index = i;
			break;
		}
	}
	if (index == -1) return USERNAME_NOTFOUND;
	if (listAcc[index].status == BLOCKED) return ACC_BLOCKED;
	return index;
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
Function do with one client
IN int index: index of soocket connect to client
as index of section contain client state
IN char *buff: buffer use to transfer
IN int buffSize: size of buffer
Return SOCKET_ERROR if connect error
Else return number of byte send msg to client
*/
int processClient(int index, char *buff, int buffSize) {
	int ret;
	ret = recvExt(client[index], buff, buffSize);
	if (ret <= 0) return SOCKET_ERROR;
	char opCode = buff[0];
	buff[ret] = '\0';
	char status = listSect[index].status;
	if (opCode == USERNAME_MSG && status == UNDEFINED) {
		ret = usernameProcess(index, buff, buffSize);
	}
	else if (opCode == PASSWORD_MSG && status == DEFINED) {
		ret = passwordProcess(index, buff, buffSize);
	}
	else if (opCode == LOGOUT_MSG && status == LOGINED) {
		ret = logoutProcess(index, buff, buffSize);
	}
	else {
		//Access not found
		opCode = ACCESS_DENIED;
		//Send msg to client
		buff[0] = opCode;
		ret = sendExt(client[index], buff, 1);
	}
	return ret;
}

/*
Function process if client send usernme message
IN int index index of client connect to socket
IN char *buff: buffer use to transfer
IN int buffSize: size of buffer
Return SOCKET_ERROR if connection error
Else return number of byte send to client. (1)
*/
int usernameProcess(int index, char *buff, int buffSize) {
	int indexOfAcc = getIndexByUsn(&buff[1]);
	if (indexOfAcc == ACC_BLOCKED) {
		buff[0] = ACC_BLOCKED;
	}
	else if (indexOfAcc == USERNAME_NOTFOUND) {
		buff[0] = USERNAME_NOTFOUND;
	}
	else {
		buff[0] = USERNAME_CORRECT;
		listSect[index].status = DEFINED;
		listSect[index].indexInListAcc = indexOfAcc;
		listSect[index].countIncoPass = 0;
	}
	//Send msg contain opcode to client
	return sendExt(client[index], buff, 1);
}

/*
Function process if client send password message
IN int index index of client connect to socket
IN char *buff: buffer use to transfer
IN int buffSize: size of buffer
Return SOCKET_ERROR if connection error
Else return number of byte send msg to client. (1)
*/
int passwordProcess(int index, char *buff, int buffSize) {
	int indexInListAcc = listSect[index].indexInListAcc;
	char *pass = &buff[1];
	char *correctPass = listAcc[listSect[index].indexInListAcc].password;
	if (strcmp(pass, correctPass) == 0) {
		buff[0] = PASSWORD_CORRECT;
		listSect[index].countIncoPass = 0;
	}
	else {
		//Password incorect
		if (listSect[index].countIncoPass == 3) {
			//Update status of account
			int indexInListAcc = listSect[index].indexInListAcc;
			listAcc[indexInListAcc].status = BLOCKED;
			updateFileAcc(listAcc, listLen, ACC_FILE);
			
			//Update status of section
			listSect[index].status = UNDEFINED;
			buff[0] = ACC_BLOCKED;
		}
		else if (listAcc[indexInListAcc].status == BLOCKED) {
			buff[0] = ACC_BLOCKED;
		}
		else{
			listSect[index].countIncoPass++;
			buff[0] = PASSWORD_INCORRECT;
		}
	}
	//Send msg contain opcode to client
	return sendExt(client[index], buff, 1);
}

/*
Funciton process if client send logout message
IN int index index of client connect to socket
IN char *buff: buffer use to transfer
IN int buffSize: size of buffer
Return SOCKET_ERROR if connection error
Else return number of byte send msg to client. (1)
*/
int logoutProcess(int index, char *buff, int buffSize) {
	listSect[index].status = UNDEFINED;
	buff[0] = LOGOUT_SUCCESS;
	//Send msg to client
	return sendExt(client[index], buff, 1);
}