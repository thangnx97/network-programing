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


typedef struct Section {
	char username[USERNAME_SIZE];
	SOCKET connSock;
}Section, *pSection;

//Create section list
Section sectList[MAX_SECTIONS];
int sectCount = 0;

/*
Function check if user is logined by another client
IN char * userName, user name of client
Return 0 if user is not logined
Else return 1
*/
int checkLogined(char * userName) {
	for (int i = 0; i < sectCount; i++) {
		if (strcmp(userName, sectList[i].username) == 0) {
			return 1;
		}
	}
	return 0;
}

/*
Function add an section to section list
Use after user login success
Return 0 if success
Else return 1
*/
int addSect(Section sect) {
	if (sectCount == MAX_SECTIONS) return 1;
	sectList[sectCount] = sect;
	sectCount++;
	return 0;
}

/*
Remove section in section list
IN char * userName
*/
int removeSect(char *userName) {
	int index;
	for (index = 0; index < sectCount; index++) {
		if (strcmp(sectList[index].username, userName) == 0) break;
	}
	if (index == sectCount) return 1;
	for (; index < sectCount; index++) {
		sectList[index] = sectList[index + 1];
	}
	sectCount--;
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

//Define account struct
typedef struct Sccount {
	char username[USERNAME_SIZE];
	char password[PASSWORD_SIZE];
	char status;
} Acc;

Acc *listAcc;
int listLen;


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
IN: an account array by pointer
IN: number of account in array, list length
IN: username
Return: USERNAME_NOTFOUND if can not find user name in account list
esle return ACC_BLOCKED if account is blocked
else return index of account with this user name
*/
int getIndexByUsn(Acc *list, int listLen, char *username) {
	int i;
	int index = -1;
	for (i = 0; i < listLen; i++) {
		if (strcmp(username, list[i].username) == 0) {
			index = i;
			break;
		}
	}
	if (index == -1) return USERNAME_NOTFOUND;
	if (list[index].status == BLOCKED) return ACC_BLOCKED;
	if (checkLogined(username) == 1) return ACC_LOGINED;
	return index;
}

/*
Function use to connect to a client
And process login, logout request
*/
unsigned __stdcall loginThread(void *param) {
	SOCKET connSock = (SOCKET)param;
	char buff[BUFF_SIZE];
	int disconnect = 0;
	int ret;
	char optCode;

	/*
	printf("Connected to [%s:%u]\n",
	inet_ntoa(clientAddr.sin_addr),
	ntohs(clientAddr.sin_port));
	*/

	int state = UNDEFINED;
	int index = -1;
	int countIncoPass = 0;

	while (disconnect != 1) {
		//Get message from client
		ret = recvExt(connSock, buff, BUFF_SIZE);
		if (ret == SOCKET_ERROR) {
			printf("Error code: %d\n", WSAGetLastError());
			disconnect = 1;
		}
		else {
			optCode = buff[0];
			buff[ret] = '\0';
			if (optCode == USERNAME_MSG && state == UNDEFINED) {
				//Check user name
				ret = getIndexByUsn(listAcc, listLen, &buff[1]);
				if (ret == USERNAME_NOTFOUND || ret == ACC_BLOCKED 
					|| ret == ACC_LOGINED) {
					//Send msg USERNAME_NOTFOUND to client
					char code = ret;
					ret = sendExt(connSock, &code, 1);
					if (ret == SOCKET_ERROR) disconnect = 1;
				}
				else {
					index = ret;
					state = DEFINED;
					char code = USERNAME_CORRECT;
					ret = sendExt(connSock, &code, 1);
					if (ret == SOCKET_ERROR) disconnect = 1;
				}
			}
			else if (optCode == PASSWORD_MSG && state == DEFINED) {
				//Chek password and logined in another client
				if (strcmp(&buff[1], listAcc[index].password) == 0
					&& checkLogined(listAcc[index].username)==0) {
					optCode = PASSWORD_CORRECT;
					//Init section to add
					Section sect;
					strcpy_s(sect.username, listAcc[index].username);
					sect.connSock = connSock;
					//Add section to list section
					addSect(sect);

					countIncoPass = 0;
					state = LOGINED;
				}
				else {
					//Check logined 
					if (checkLogined(listAcc[index].username) == 1) {
						state = UNDEFINED;
						optCode = ACC_LOGINED;
					}
					else {
						//Incorrect
						optCode = PASSWORD_INCORRECT;
						countIncoPass++;
						//Check incorrect 3 time
						if (countIncoPass > 3) {
							listAcc[index].status = BLOCKED;
							updateFileAcc(listAcc, listLen, ACC_FILE);
							state = UNDEFINED;
							countIncoPass = 0;
							optCode = ACC_BLOCKED;
						}
					}
				}
				//Send result to client
				ret = sendExt(connSock, &optCode, 1);
				if (ret == SOCKET_ERROR) disconnect = 1;
			}
			else if (optCode == LOGOUT_MSG && state == LOGINED) {
				//Logout
				optCode = LOGOUT_SUCCESS;
				state = UNDEFINED;
				countIncoPass = 0;
				removeSect(listAcc[index].username);
				index = -1;
				ret = sendExt(connSock, &optCode, 1);
				if (ret == SOCKET_ERROR) disconnect = 1;
			}
		}
	}
	if (state == LOGINED && index != -1) {
		//If connecttion error but user not logout
		//Remove section from section list
		removeSect(listAcc[index].username);
	}
	closesocket(connSock);
	return 0;
}


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
		int disconnect = 0;
		connSock = accept(sever, (sockaddr*)&clientAddr, &clientAddrLen);
		//Create new thread
		_beginthreadex(0, 0, loginThread, (void*)connSock, 0, 0);
	}
	//End of ussing socket sever
	closesocket(sever);
	//End of ussing winsock
	WSACleanup();
	return 0;
}