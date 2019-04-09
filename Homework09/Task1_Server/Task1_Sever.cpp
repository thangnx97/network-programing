#include <stdio.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <string.h>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <process.h>
#include "mydef.h"
#include "mynetopt.h"
#pragma comment(lib, "Ws2_32.lib")



//Deffine state 
#define ACC_FILE "account.txt"
#define MAX_ACC_COUNT 100
#define MAX_THREAD 100

//Define section struct
typedef struct Section{
	WSAOVERLAPPED overlap;
	DWORD recvBytes;
	DWORD sendBytes;
	DWORD toSendBytes;
	char buff[BUFF_SIZE];
	WSABUF dataBuff;
	char operation;
	char countIncoPass;
	char status;
	int indexInListAcc;
} Section, *pSection;

typedef struct SockStruct {
	SOCKET sock;
}SockS, *pSockS;

//Define account struct
typedef struct Account {
	char username[USERNAME_SIZE];
	char password[PASSWORD_SIZE];
	char status;
} Acc;

//Stuct contain information of each thread


/*Global variable*/
//Array contain users info and length of it
Acc *listAcc;
int listLen;

//A criticalsection to access acc list and acc file
CRITICAL_SECTION accCrt;

//Define functions
int splitAcc(char *str, Acc* pAcc);
int readFileToList(Acc **list, const char * fileName);
int updateFileAcc(Acc *list, int listLen, char *fileName);
int checkArgs(int argc, char ** argv, int *port);
int getIndexByUsn(char *username);
int usernameProcess(pSection pSect);
int passwordProcess(pSection pSect);
int logoutProcess(pSection pSect);
unsigned __stdcall workerThread(LPVOID lpParam);
int processRecvMsg(pSection pSect);
void initSection(pSection pSect);
void closeSection(pSection pSect, pSockS pConn);
void resetToSend(pSection pSect);
void resetToRecv(pSection pSect);


//Main function begin
int main(int argc, char **argv) {
	
	/* Setup argument and winsock*/
	int port;
	if (checkArgs(argc, argv, &port) == 1) return 1;

	//Init criticalsection
	InitializeCriticalSection(&accCrt);

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
		threads[i] =(HANDLE) _beginthreadex(NULL, 0, workerThread, (void*)completionPort, 0, 0);
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
	WaitForMultipleObjects(sysInfo.dwNumberOfProcessors, threads, TRUE, INFINITE);
	WSACleanup();
	return 0;
}

/*********************************************************/

/*
Thread to transfer to client
*/
unsigned __stdcall workerThread(LPVOID parma) {
	HANDLE compPort = (HANDLE)parma;
	DWORD transBytes;
	DWORD flags = 0;
	pSection pSect;
	pSockS pConnSock;
	while (TRUE) {
		//Get quence status
		if (GetQueuedCompletionStatus(compPort, &transBytes,(LPDWORD) &pConnSock,
			(LPOVERLAPPED*)&pSect, INFINITE) == 0) {
			printf("Thread have an error in get quence status\n");
			continue;
		}

		if (transBytes == 0) {
			//Connect closed
			closeSection(pSect, pConnSock);
			continue;
		}
		
		if (pSect->operation == RECV) {
			pSect->buff[transBytes] = '\0';
			pSect->operation = SEND;
			pSect->recvBytes = transBytes;
			pSect->sendBytes = 0;
			pSect->toSendBytes = processRecvMsg(pSect);
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
			if (WSARecv(pConnSock->sock, &pSect->dataBuff, 1, &transBytes, &flags, &pSect->overlap, NULL) == 0) {
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
Function reset section to send
Zero ovelap and setup the dataBuff
INPUT: a pointer point to sectionn
*/
void resetToSend(pSection pSect) {
	ZeroMemory(&pSect->overlap, sizeof(OVERLAPPED));
	pSect->dataBuff.buf = pSect->buff + pSect->sendBytes;
	pSect->dataBuff.len = pSect->toSendBytes - pSect->sendBytes;
}

/*
Function reset section to receive
Zero ovelap and setup the dataBuff
INPUT: a pointer point to sectionn
*/
void resetToRecv(pSection pSect) {
	ZeroMemory(&pSect->overlap, sizeof(OVERLAPPED));
	pSect->operation = RECV;
	pSect->sendBytes = 0;
	pSect->recvBytes = 0;
	pSect->dataBuff.buf = pSect->buff;
	pSect->dataBuff.len = BUFF_SIZE;
}

/*
Init section function, not alloc memory space
Function init section at index i in sections array
And init event at index i in events array
INPUT: a pointer point to sectionn
*/
void initSection(pSection pSect) {
	ZeroMemory(&pSect->overlap, sizeof(WSAOVERLAPPED));
	pSect->sendBytes = 0;
	pSect->dataBuff.buf = pSect->buff;
	pSect->dataBuff.len = BUFF_SIZE;
	pSect->status = UNDEFINED;
	pSect->operation = RECV;
}

/*
Funtion close section poited by pSect
And free it
INPUT: a pointer point to sectionn
*/
void closeSection(pSection pSect, pSockS pConn) {
	closesocket(pConn->sock);
	GlobalFree(pConn);
	GlobalFree(pSect);
}

/*
Function split string contain username, password and status
to an account struct
INPUT: string contain username password and status
INPUT/OUTPUT: array contain accouts struct
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
INPUT: a const string of fileName
OUTPUT: Acc **list a pointer to list of Account
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
Function process msg from client
And put result to buffer to send to client
INPUT/OUTPUT: a poiter point to section struct
*/
int processRecvMsg(pSection pSect) {
	char opCode = pSect->buff[0];
	char status =pSect->status;
	if (opCode == USERNAME_MSG && status == UNDEFINED) {
		usernameProcess(pSect);
	}
	else if (opCode == PASSWORD_MSG && status == DEFINED) {
		passwordProcess(pSect);
	}
	else if (opCode == LOGOUT_MSG && status == LOGINED) {
		logoutProcess(pSect);
	}
	else {
		//Access not found
		opCode = ACCESS_DENIED;
		pSect->buff[0] = opCode;
	}
	pSect->toSendBytes = 1;
	return 1;
}

/*
Function process if client send usernme message
And put result msg to buffer
INPUT/OUTPUT: a poiter point to section struct
Return code send to client
*/
int usernameProcess(pSection pSect) {
	int indexOfAcc = getIndexByUsn(&pSect->buff[1]);
	if (indexOfAcc == ACC_BLOCKED) {
		pSect->buff[0] = ACC_BLOCKED;
	}
	else if (indexOfAcc == USERNAME_NOTFOUND) {
		pSect->buff[0] = USERNAME_NOTFOUND;
	}
	else {
		pSect->buff[0] = USERNAME_CORRECT;
		pSect->status = DEFINED;
		pSect->indexInListAcc = indexOfAcc;
		pSect->countIncoPass = 0;
	}
	return pSect->buff[0];
}

/*
Function process if client send password message
And put result msg to buffer
INPUT/OUTPUT: a poiter point to section struct
Return code send to client
*/
int passwordProcess(pSection pSect) {
	int indexInListAcc = pSect->indexInListAcc;
	char *pass = &pSect->buff[1];
	char *correctPass = listAcc[pSect->indexInListAcc].password;
	if (strcmp(pass, correctPass) == 0) {
		pSect->status = LOGINED;
		pSect->buff[0] = PASSWORD_CORRECT;
		pSect->countIncoPass = 0;
	}
	else {
		//Password incorect
		if (pSect->countIncoPass == 3) {
			//Update status of account
			int indexInListAcc = pSect->indexInListAcc;
			
			EnterCriticalSection(&accCrt);
			listAcc[indexInListAcc].status = BLOCKED;
			updateFileAcc(listAcc, listLen, ACC_FILE);
			LeaveCriticalSection(&accCrt);
			
			//Update status of section
			pSect->status = UNDEFINED;
			pSect->buff[0] = ACC_BLOCKED;
		}
		else if (listAcc[indexInListAcc].status == BLOCKED) {
			pSect->buff[0] = ACC_BLOCKED;
			pSect->status = UNDEFINED;
		}
		else{
			pSect->countIncoPass++;
			pSect->buff[0] = PASSWORD_INCORRECT;
		}
	}
	//Send msg contain opcode to client
	return pSect->buff[0];
}

/*
Funciton process if client send logout messageF
And put result msg to buffer
INPUT/OUTPUT: a poiter point to section struct
Return code send to client
*/
int logoutProcess(pSection pSect) {
	pSect->status = UNDEFINED;
	pSect->buff[0] = LOGOUT_SUCCESS;
	//Send msg to client
	return pSect->buff[0];
}