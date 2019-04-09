// Task1_Server.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "Task1_Server.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "winsock2.h"
#include "windows.h"
#include "stdio.h"
#include "mydefine.h"
#include "mynetopt.h"

#pragma comment(lib, "Ws2_32.lib")

#define MAX_LOADSTRING 100
#define WM_SOCKET WM_USER + 1
#define SERVER_PORT 5500
#define MAX_CLIENT 1024

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

Acc* listAcc;
Section listSect[MAX_SECTIONS];
int listLen;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
HWND               InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

//Define some functions
int splitAcc(char *str, Acc* pAcc);
int readFileToList(Acc **list, const char * fileName);
int updateFileAcc(Acc *list, int listLen, char *fileName);
int checkArgs(int argc, char ** argv, int *port);
int getIndexByUsn(char *username);
int processClient(int index, char *buff, int buffSize);
int usernameProcess(int index, char *buff, int buffSize);
int passwordProcess(int index, char *buff, int buffSize);
int logoutProcess(int index, char *buff, int buffSize);

/*
Function reset section at index i of listSection
*/
void resetSection(int i) {
	if (i < 0 || i >= MAX_SECTIONS) return;
	closesocket(listSect[i].sock);
	listSect[i].countIncoPass = 0;
	listSect[i].indexInListAcc = -1;
	listSect[i].status = UNDEFINED;
	listSect[i].sock = 0;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow){

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.
	


    // Initialize global strings
	MSG msg;
	HWND serverWindow;
	
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_TASK1_SERVER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if ((serverWindow = InitInstance (hInstance, nCmdShow))==NULL){
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TASK1_SERVER));

	//Initiate WinSock
	WSADATA wsaData;
	WORD wVersion = MAKEWORD(2, 2);
	if (WSAStartup(wVersion, &wsaData)) {
		MessageBox(serverWindow, L"Cannot listen!", L"Error!", MB_OK);
		return 0;
	}
	SOCKET listenSock;
	//Construct socket	
	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	//Requests Windows message-based notification of network events for listenSock
	WSAAsyncSelect(listenSock, serverWindow, WM_SOCKET, FD_ACCEPT | FD_CLOSE | FD_READ);

	//Bind address to socket
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(listenSock, (sockaddr *)&serverAddr, sizeof(serverAddr))){
		MessageBox(serverWindow, L"Cannot bind!", L"Error!", MB_OK);
	}

	//Listen request from client
	if (listen(listenSock, MAX_CLIENT)) {
		MessageBox(serverWindow, L"Cannot listen!", L"Error!", MB_OK);
		return 0;
	}



    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0)){
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)){
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TASK1_SERVER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_TASK1_SERVER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
HWND InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

	//Init listAcc array

   listLen = readFileToList(&listAcc, ACC_FILE);
	//Init listSect array
	for (int i = 0; i < MAX_SECTIONS; i++) {
		resetSection(i);
	}

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd){
      return FALSE;
	}

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return hWnd;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam){
	char buff[BUFF_SIZE];
	int i;
	SOCKET connSock;
	sockaddr_in clientAddr;
	int clientAddrLen = sizeof(clientAddr);
	
	switch (message) {
	case WM_SOCKET: 
		if (WSAGETSELECTERROR(lParam)){ 
			for (i = 0; i < MAX_CLIENT; i++) {
				if (listSect[i].sock == (SOCKET)wParam) {
					resetSection(i);
					continue;
				}
			}
		}
		//Do not have error
		switch (WSAGETSELECTEVENT(lParam)) {
		case FD_ACCEPT:
		{
			connSock = accept((SOCKET)wParam, (sockaddr *)&clientAddr, &clientAddrLen);
			if (connSock == INVALID_SOCKET) {
				break;
			}
			for (i = 0; i < MAX_CLIENT; i++)
				if (listSect[i].sock == 0) {
					listSect[i].sock = connSock;
					//Requests Windows message-based notification of network events for connSock
					WSAAsyncSelect(connSock, hWnd, WM_SOCKET, FD_READ | FD_CLOSE);
					break;
				}
			if (i == MAX_CLIENT)
				MessageBox(hWnd, L"Too many clients!", L"Notice", MB_OK);
		}
		break;

		case FD_READ:
		{
			for (i = 0; i < MAX_CLIENT; i++)
				if (listSect[i].sock == (SOCKET)wParam)
					break;
			processClient(i, buff, BUFF_SIZE);
		}
		break;

		case FD_CLOSE:
			for (i = 0; i < MAX_CLIENT; i++)
				if (listSect[i].sock == (SOCKET)wParam) {
					resetSection(i);
					break;
				}
		break;
	}
	break;

    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}


/*************************************/
//Some function of login program
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
	ret = recvExt(listSect[index].sock, buff, buffSize);
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
		ret = sendExt(listSect[index].sock, buff, 1);
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
	return sendExt(listSect[index].sock, buff, 1);
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
		else {
			listSect[index].countIncoPass++;
			buff[0] = PASSWORD_INCORRECT;
		}
	}
	//Send msg contain opcode to client
	return sendExt(listSect[index].sock, buff, 1);
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
	return sendExt(listSect[index].sock, buff, 1);
}
