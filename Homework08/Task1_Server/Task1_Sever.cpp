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
#define MAX_THREAD 500 
#define MAX_CLIENT (MAX_THREAD * WSA_MAXIMUM_WAIT_EVENTS)

//Define section struct
typedef struct Section{
	WSAOVERLAPPED overlap;
	SOCKET sock;
	DWORD recvByte;
	DWORD sendByte;
	char buff[BUFF_SIZE];
	WSABUF dataBuff;
	char operation;
	char countIncoPass;
	char status;
	int indexInListAcc;
} Section, *pSection;

//Define account struct
typedef struct Account {
	char username[USERNAME_SIZE];
	char password[PASSWORD_SIZE];
	char status;
} Acc;

//Stuct contain information of each thread
typedef struct ThreadInfo {
	WSAEVENT *tEvents;
	CRITICAL_SECTION criticle;
	int indexOfThread;
	int nEvent;
}ThreadInfo, *pThreadInfo;


/*Global variable*/
//sections array and events array
Section* sections[MAX_CLIENT];
WSAEVENT events[MAX_CLIENT];

//To manage threads and number of thread
ThreadInfo threads[MAX_THREAD];
HANDLE threadHandle[MAX_THREAD];
int nThread;

//Array contain users info and length of it
Acc *listAcc;
int listLen;

//Number of client communicate at the moment;
int nEvent = 0;

//Crictiale section to multithreads sync
CRITICAL_SECTION accCrt;
CRITICAL_SECTION sectCrt;
CRITICAL_SECTION threadCrt;

//Define functions
int splitAcc(char *str, Acc* pAcc);
int readFileToList(Acc **list, const char * fileName);
int updateFileAcc(Acc *list, int listLen, char *fileName);
int checkArgs(int argc, char ** argv, int *port);
int getIndexByUsn(char *username);
int usernameProcess(int index, char *buff, int buffSize);
int passwordProcess(int index, char *buff, int buffSize);
int logoutProcess(int index, char *buff, int buffSize);
unsigned __stdcall workerThread(LPVOID lpParam);
int processRecvMsg(int index, char *buff, int buffSize, DWORD *byteSend);
void initSection(int i, SOCKET connSock);
void resetSection(int i);
void sectionCpy(int dest, int source);
void closeSection(int i);
void closeSectionEx(int idSect, ThreadInfo* pThInfo);


//Main function begin
int main(int argc, char **argv) {

	/* Setup argument and winsock*/
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

	//Init listen socket 
	SOCKET listenSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (listenSock == INVALID_SOCKET) {
		printf("Can not init socket.\n");
		WSACleanup();
		return 1;
	}

	//Init sever address usse INADDR_ANY argument...
	sockaddr_in severAddr;
	severAddr.sin_family = AF_INET;
	severAddr.sin_addr.s_addr = htonl(ADDR_ANY);
	severAddr.sin_port = htons(port);

	sockaddr_in clientAddr;
	int clientAddrLen = sizeof(clientAddr);

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

	//Create event to listenSocket
	WSAEVENT listenEvent;
	if((listenEvent= WSACreateEvent()) == WSA_INVALID_EVENT) {
		//Show error and exit
		printf("WSACreateEvent() failed with error: %d\n", WSAGetLastError());
		closesocket(listenSock);
		WSACleanup();
		return 1;
	}

	//Initialze cricticle sections
	InitializeCriticalSection(&accCrt);
	InitializeCriticalSection(&threadCrt);
	InitializeCriticalSection(&sectCrt);

	//Init WSA_MAXIMUM_WAIT_EVENTS for first thread
	for (int i = 0; i < MAX_CLIENT; i++) {
		sections[i]= NULL;
	}
	for (int i = 0; i < MAX_THREAD; i++) {
		threadHandle[i] = 0;
	}
	nThread = 0;

	int index;
	SOCKET connSock;
	DWORD flags = 0;
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
			//Find space of new section
			EnterCriticalSection(&sectCrt);
			for (index = 0; index < MAX_CLIENT; index++) {
				if (sections[index] == NULL) break;
			}
			LeaveCriticalSection(&sectCrt);
			if (index == MAX_CLIENT) {
				//To many client connect to sever
				printf("Too many client\n");
				continue;
			}
			else {
				int indexOfThread = index / WSA_MAXIMUM_WAIT_EVENTS;
				printf("Connect to client with socket: %d\n", connSock);
				
				//Create section and event to new client
				initSection(index, connSock);

				//Post Receive request
				WSARecv(sections[index]->sock, &sections[index]->dataBuff, 1, &sections[index]->recvByte, &flags, &sections[index]->overlap, NULL);
				
				//Check if client have no thread to manage
				if (threadHandle[indexOfThread]== 0) {
					EnterCriticalSection(&threadCrt);
					//Create new thread
					InitializeCriticalSection(&threads[indexOfThread].criticle);
					threads[indexOfThread].nEvent = 1;
					threads[indexOfThread].tEvents = &events[indexOfThread*WSA_MAXIMUM_WAIT_EVENTS];
					threads[indexOfThread].indexOfThread = indexOfThread;
					threadHandle[indexOfThread] = (HANDLE)_beginthreadex(0, 0, workerThread, (LPVOID)&threads[indexOfThread], 0, 0);
					nThread++;
					LeaveCriticalSection(&threadCrt);
					
				}
				else {
					//Thread manage client index is already exist
					//EnterCricticleSection
					//Must fix it
					EnterCriticalSection(&threads[indexOfThread].criticle);
					threads[indexOfThread].nEvent++;
					LeaveCriticalSection(&threads[indexOfThread].criticle);
				}
				
			}
		}
	}

	//Wait for all thread done
	WaitForMultipleObjects(nThread, threadHandle, TRUE, INFINITE);
	WSACleanup();
	return 0;
}

/*********************************************************/

/*
Thread to transfer to client
*/
unsigned __stdcall workerThread(LPVOID parma) {
	ThreadInfo *pThInfo = (ThreadInfo*)parma;

	int index;
	int idx;
	int idxSect;//Index in section array
	DWORD transByte;
	DWORD flags = 0;
	while (TRUE) {
		//Wait for event signed	
		index = WSAWaitForMultipleEvents(pThInfo->nEvent, pThInfo->tEvents, FALSE, 200, FALSE);
		if (index == WSA_WAIT_TIMEOUT) {
			continue;
		}
		if (index == WSA_WAIT_FAILED) {
			//Check if nEvent == 0
			if (pThInfo->nEvent == 0) {
				SleepEx(1000, FALSE);
				//Set thread to sleep
				//Must fix
				continue;
			}
		}
		else {
			//Have event in client
			index -= WSA_WAIT_EVENT_0;
			//Check event from index to nEvent of thread
			for (int i = index; i < pThInfo->nEvent; i++) {
				idx = WSAWaitForMultipleEvents(1, pThInfo->tEvents + i, FALSE, 10, FALSE);
				if (idx != WSA_WAIT_FAILED && idx != WSA_WAIT_TIMEOUT) {
					idx -= WSA_WAIT_EVENT_0;
					//Define index of  client in the section array
					idxSect = i + idx + pThInfo->indexOfThread * WSA_MAXIMUM_WAIT_EVENTS;

					WSAGetOverlappedResult(sections[idxSect]->sock, &sections[idxSect]->overlap, &transByte, FALSE, &flags);

					if (transByte == 0) {
						//
						closeSectionEx(i + idx, pThInfo);
						continue;
					}
					else {
						//Tranfer success
						if (sections[idxSect]->operation == RECV) {
							//Process msg from client
							sections[idxSect]->buff[transByte] = '\0';
							processRecvMsg(idxSect, sections[idxSect]->buff, BUFF_SIZE, &sections[idxSect]->sendByte);
							//Send msg to client, usse WSASend()
							sections[idxSect]->operation = SEND;
							ResetEvent(events[idxSect]);
							if (WSASend(sections[idxSect]->sock, &sections[idxSect]->dataBuff, 1, &sections[idxSect]->sendByte,
								flags, &sections[idxSect]->overlap, NULL) == SOCKET_ERROR) {
								if (WSAGetLastError() != WSA_IO_PENDING) {
									//Hve error in transfer
									closeSectionEx(i + idx, pThInfo);
									continue;
								}
							}
						}
						else {
							//Send 1 byte msg success request to receive msg from client
							resetSection(idxSect);
							flags = 0;
							//Post Recv request
							if (WSARecv(sections[idxSect]->sock, &sections[idxSect]->dataBuff, 1, &transByte, &flags, &sections[idxSect]->overlap, NULL) == SOCKET_ERROR) {
								if (WSAGetLastError() != WSA_IO_PENDING) {
									//Have error in the socket index
									printf("WSARecv() failed with error %d\n", WSAGetLastError());
									closeSectionEx(i + idx, pThInfo);
								}
							}
						}
					}
				}
			}
		}
	}
	return 0;
}

/*
Function close section
And take the array clean
IN index in thread of socket to close
IN pointer to threadinfo contain info of thead
*/
void closeSectionEx(int idSect, ThreadInfo* pThInfo) {
	int idSectInArr = idSect + pThInfo->indexOfThread*WSA_MAXIMUM_WAIT_EVENTS;
	EnterCriticalSection(&threadCrt);
	closeSection(idSectInArr);
	EnterCriticalSection(&pThInfo->criticle);
	pThInfo->nEvent--;
	int idSectFinish = pThInfo->nEvent + pThInfo->indexOfThread*WSA_MAXIMUM_WAIT_EVENTS;
	LeaveCriticalSection(&pThInfo->criticle);
	if (idSectInArr < idSectFinish) {
		EnterCriticalSection(&sectCrt);
		sectionCpy(idSectInArr, idSectFinish);
		LeaveCriticalSection(&sectCrt);
	}
	LeaveCriticalSection(&threadCrt);
}

/*
Init section function
Function init section at index i in sections array
And init event at index i in events array
*/
void initSection(int i, SOCKET connSock) {
	sections[i] = (Section*)GlobalAlloc(GPTR, sizeof(Section));
	sections[i]->sock = connSock;
	ZeroMemory(&sections[i]->overlap, sizeof(WSAOVERLAPPED));
	events[i] = WSACreateEvent();
	sections[i]->sendByte = 0;
	sections[i]->dataBuff.buf = sections[i]->buff;
	sections[i]->dataBuff.len = BUFF_SIZE;
	sections[i]->status = UNDEFINED;
	sections[i]->operation = RECV;
	sections[i]->overlap.hEvent = events[i];
	ResetEvent(events[i]);
}

/*
Function reset section to take WSARecv() request
*/
void resetSection(int i) {
	ZeroMemory(&sections[i]->overlap, sizeof(WSAOVERLAPPED));
	sections[i]->overlap.hEvent = events[i];
	sections[i]->sendByte = 0;
	sections[i]->operation = RECV;
	sections[i]->dataBuff.buf = sections[i]->buff;
	sections[i]->dataBuff.len = BUFF_SIZE;
	//Reset event
	ResetEvent(events[i]);
}

/*
Funtion close section at index i in sections array
And close event at index i in events array
*/
void closeSection(int i) {
	closesocket(sections[i]->sock);
	WSACloseEvent(events[i]);
	GlobalFree(sections[i]);
	sections[i] = NULL;
}

/*
Function copy section at index source
To index dest in the sections array
Do the same thing with events array
*/
void sectionCpy(int dest, int source) {

	events[dest] = events[source];
	events[source] = 0;
	sections[dest] = sections[source];
	sections[source] = NULL;
}

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
Function process msg from client
And put result to buffer to send to client
IN int index: index of client section in sections array
as index of client event in events array
IN/OUT char *buff: buffer use to transfer
IN int buffSize: size of buffer
Return byte of msg result to send to client (1)
*/
int processRecvMsg(int index, char *buff, int buffSize, DWORD *sendByte) {
	char opCode = buff[0];
	char status = sections[index]->status;
	if (opCode == USERNAME_MSG && status == UNDEFINED) {
		usernameProcess(index, buff, buffSize);
	}
	else if (opCode == PASSWORD_MSG && status == DEFINED) {
		passwordProcess(index, buff, buffSize);
	}
	else if (opCode == LOGOUT_MSG && status == LOGINED) {
		logoutProcess(index, buff, buffSize);
	}
	else {
		//Access not found
		opCode = ACCESS_DENIED;
		buff[0] = opCode;
	}
	*sendByte = 1;
	return 1;
}

/*
Function process if client send usernme message
And put result msg to buffer
IN int index index of client connect to socket
IN/OUT char *buff: buffer use to transfer
IN int buffSize: size of buffer
Return code send to client
*/
int usernameProcess(int index, char *buff, int buffSize) {
	EnterCriticalSection(&accCrt);
	int indexOfAcc = getIndexByUsn(&buff[1]);
	LeaveCriticalSection(&accCrt);
	if (indexOfAcc == ACC_BLOCKED) {
		buff[0] = ACC_BLOCKED;
	}
	else if (indexOfAcc == USERNAME_NOTFOUND) {
		buff[0] = USERNAME_NOTFOUND;
	}
	else {
		buff[0] = USERNAME_CORRECT;
		sections[index]->status = DEFINED;
		sections[index]->indexInListAcc = indexOfAcc;
		sections[index]->countIncoPass = 0;
	}
	return buff[0];
}

/*
Function process if client send password message
And put result msg to buffer
IN int index index of client connect to socket
IN/OUT char *buff: buffer use to transfer
IN int buffSize: size of buffer
Return code send to client
*/
int passwordProcess(int index, char *buff, int buffSize) {
	int indexInListAcc = sections[index]->indexInListAcc;
	char *pass = &buff[1];
	char *correctPass = listAcc[sections[index]->indexInListAcc].password;
	if (strcmp(pass, correctPass) == 0) {
		sections[index]->status = LOGINED;
		buff[0] = PASSWORD_CORRECT;
		sections[index]->countIncoPass = 0;
	}
	else {
		//Password incorect
		if (sections[index]->countIncoPass == 3) {
			//Update status of account
			int indexInListAcc = sections[index]->indexInListAcc;
			
			EnterCriticalSection(&accCrt);
			listAcc[indexInListAcc].status = BLOCKED;
			updateFileAcc(listAcc, listLen, ACC_FILE);
			LeaveCriticalSection(&accCrt);
			
			//Update status of section
			sections[index]->status = UNDEFINED;
			buff[0] = ACC_BLOCKED;
		}
		else if (listAcc[indexInListAcc].status == BLOCKED) {
			buff[0] = ACC_BLOCKED;
		}
		else{
			sections[index]->countIncoPass++;
			buff[0] = PASSWORD_INCORRECT;
		}
	}
	//Send msg contain opcode to client
	return buff[0];
}

/*
Funciton process if client send logout messageF
And put result msg to buffer
IN int index index of client connect to socket
IN/OUT char *buff: buffer use to transfer
IN int buffSize: size of buffer
Return code send to client
*/
int logoutProcess(int index, char *buff, int buffSize) {
	sections[index]->status = UNDEFINED;
	buff[0] = LOGOUT_SUCCESS;
	//Send msg to client
	return buff[0];
}