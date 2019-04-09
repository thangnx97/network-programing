#include "stdafx.h"
#include "Task2_Server.h"
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
#include "mydefine.h"

#pragma comment(lib, "Ws2_32.lib")

#define MAX_LOADSTRING 100
#define SERVER_PORT 5500
#define MAX_CLIENTS 20
#define WM_SOCKET WM_USER + 1

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
Section listSect[MAX_CLIENTS];



// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
HWND                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
int getRandomFileName(char *fileName, int size);
int encodeBlock(char* buff, int buffSize, unsigned char key);
int decodeBlock(char* buff, int buffSize, unsigned char key);
int sendErrorCodeToClient(SOCKET connSock);
void resetSect(int idx);
int processEncodeMSG(int idx, char *buff, int buffSize);
int processDecodeMSG(int idx, char *buff, int buffSize);
int processSendFile(int idx, char *buff, int buffSize);
int processRecvFile(int idx, char *buff, int buffSize);
int processClient(int index, char *buff, int buffSize);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
	MSG msg;
	HWND serverWindow;
    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_TASK2_SERVER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if ((serverWindow = InitInstance (hInstance, nCmdShow))==NULL)
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TASK2_SERVER));

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

	if (bind(listenSock, (sockaddr *)&serverAddr, sizeof(serverAddr))) {
		MessageBox(serverWindow, L"Cannot bind!", L"Error!", MB_OK);
	}

	//Listen request from client
	if (listen(listenSock, MAX_CLIENTS)) {
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
ATOM MyRegisterClass(HINSTANCE hInstance){
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TASK2_SERVER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_TASK2_SERVER);
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
HWND InitInstance(HINSTANCE hInstance, int nCmdShow){
   hInst = hInstance; // Store instance handle in our global variable
   //Init listSect array
   for (int i = 0; i < MAX_CLIENTS; i++) {
	   listSect[i].sock = 0;
	   listSect[i].status = UNDEFINED;
	   listSect[i].pFile = NULL;
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
	int i, ret;
	SOCKET connSock;
	sockaddr_in clientAddr;
	int clientAddrLen = sizeof(clientAddr);

	switch (message) {

	case WM_SOCKET:{
		if (WSAGETSELECTERROR(lParam)) {
			for (i = 0; i < MAX_CLIENTS; i++) {
				if (listSect[i].sock == (SOCKET)wParam) {
					resetSect(i);
					continue;
				}
			}
		}
		//Do not have error
		switch (WSAGETSELECTEVENT(lParam)) {
		case FD_ACCEPT: {
			connSock = accept((SOCKET)wParam, (sockaddr *)&clientAddr, &clientAddrLen);
			if (connSock == INVALID_SOCKET) {
				break;
			}
			for (i = 0; i < MAX_CLIENTS; i++) {
				if (listSect[i].sock == 0) {
					listSect[i].sock = connSock;
					//Requests Windows message-based notification of network events for connSock
					WSAAsyncSelect(connSock, hWnd, WM_SOCKET, FD_READ | FD_CLOSE);
					break;
				}
			}
			if (i == MAX_CLIENTS)
				MessageBox(hWnd, L"Too many clients!", L"Notice", MB_OK);
		}
		break;
		case FD_READ: {
			//Have data in socket
			for (i = 0; i < MAX_CLIENTS; i++) {
				if (listSect[i].sock == (SOCKET)wParam)
					break;
			}
			//Process with client
			ret = processClient(i, buff, BUFF_SIZE);
			if (ret <= 0) {
				//If connect is brocken
				closesocket(listSect[i].sock);
				listSect[i].sock = 0;
				resetSect(i);
			}
		}
		break;
		case FD_CLOSE: {
			//Close socket an remove from listSect
			for (i = 0; i < MAX_CLIENTS; i++) {
				if (listSect[i].sock == (SOCKET)wParam) {
					closesocket(listSect[i].sock);
					listSect[i].sock = 0;
					resetSect(i);
					break;
				}
			}
		}
		break;
		}
	}
	break;
	case WM_COMMAND:{
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
	case WM_PAINT:{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code that uses hdc here...
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY: {
		PostQuitMessage(0);
	}
	break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam){
    UNREFERENCED_PARAMETER(lParam);
    switch (message){
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL){
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}


/*
Function get randome file name in sever
Chose a random name with 4 charecter
IN.OUT char * fileName, contain name of file found
IN int size, size of fileName array
Return length of fileName, 4
*/
int getRandomFileName(char *fileName, int size) {
	srand((unsigned int)time(NULL));
	int i;
	do {
		for (i = 0; i < size - 1; i++) {
			fileName[i] = 'a' + rand() % 26;
		}
		fileName[i] = '\0';
	} while (fileExist(fileName) == FILE_EXISTED);
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
Function use to send error transfer code to client
IN SOCKET connSock, a socket connect to client
Return an value > 0 if SUCCESS
Else return SOCKET_ERROR
*/
int sendErrorCodeToClient(SOCKET connSock) {
	char s[2];
	s[0] = (char)ERROR_TRANS;
	int ret = (sendExt(connSock, s, 1));
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
		return ret;
	return 1;
}

/*
Function reset section in index idx
To status UNDEFINED
And close file pointer
Remove file in sever
*/
void resetSect(int idx) {
	//closesocket(listSect[idx].sock);
	//listSect[idx].sock = 0;
	if (listSect[idx].status != UNDEFINED) {
		listSect[idx].status = UNDEFINED;
		if (listSect[idx].pFile != NULL) fclose(listSect[idx].pFile);
		remove(listSect[idx].fileName);
	}
}

/*
Function process if msg is ENCODE msg
IN int idx: index in the section array and the client array
IN char *buff: buffer use to transfer
IN int buffSize: size of buffer
Return number of byte send to client or SOCKET_ERROR
*/
int processEncodeMSG(int idx, char *buff, int buffSize) {
	int ret;
	if (listSect[idx].status == UNDEFINED) {
		//Setup to encode
		listSect[idx].status = ENCODE;
		listSect[idx].action = RECV_FILE;
		listSect[idx].key = (unsigned char)buff[3];

		getRandomFileName(listSect[idx].fileName, 5);
		//open file to recv data
		fopen_s(&listSect[idx].pFile, listSect[idx].fileName, "wb");

		if (listSect[idx].pFile != NULL) {
			ret = ENCODE;
		}
		else {
			//printf("Client idx: %d : Can not open file to write\n", idx);
			resetSect(idx);
			ret = ERROR_TRANS;
		}
	}
	else {
		resetSect(idx);
		ret = ERROR_TRANS;
	}
	buff[0] = ret;
	return sendExt(listSect[idx].sock, buff, 1);
}

/*
Function process if msg is DECODE msg
IN int idx : index in the section array and the client array
IN char *buff : buffer use to transfer
IN int buffSize : size of buffer
Return number of byte send to client or SOCKET_ERROR
*/
int processDecodeMSG(int idx, char *buff, int buffSize) {
	int ret;
	if (listSect[idx].status == UNDEFINED) {
		//Setup to decode
		listSect[idx].status = DECODE;
		listSect[idx].action = RECV_FILE;
		listSect[idx].key = (unsigned char)buff[3];

		getRandomFileName(listSect[idx].fileName, 5);
		//open file to recv data
		fopen_s(&listSect[idx].pFile, listSect[idx].fileName, "wb");

		if (listSect[idx].pFile != NULL) {
			ret = DECODE;
		}
		else {
			//printf("Client idx: %d : Can not open file to write\n", idx);
			resetSect(idx);
			ret = ERROR_TRANS;
		}
	}
	else {
		resetSect(idx);
		ret = ERROR_TRANS;
	}
	buff[0] = ret;
	return sendExt(listSect[idx].sock, buff, 1);
}

/*
Function process if msg is TRANSFER_FILE
Sen a block of file to client
IN int idx : index in the section array and the client array
IN char *buff : buffer use to transfer
IN int buffSize : size of buffer
Return number of byte send to client or SOCKET_ERROR
*/
int processSendFile(int idx, char *buff, int buffSize) {
	if (listSect[idx].status == UNDEFINED) {
		return sendErrorCodeToClient(listSect[idx].sock);
	}

	short *pLength = (short*)&buff[1];
	int ret;

	if (listSect[idx].nLeft == 0) {
		//Send file complete, remove file on sever an reset state
		resetSect(idx);
		*pLength = 0;
		//Send to client end of transfering
		ret = sendExt(listSect[idx].sock, buff, 3);
	}
	else {
		//Read a block of file to buffer
		ret = fread(&buff[3], BLOCK_SIZE, 1, listSect[idx].pFile);
		if (ret == 1) {
			*pLength = BLOCK_SIZE;
		}
		else {
			if (listSect[idx].nLeft <= BLOCK_SIZE) {
				*pLength = (short)listSect[idx].nLeft;
			}
			else {
				//Error read file
				//printf("Can not read file on server\n");
				resetSect(idx);
				//Send error code to client
				return sendErrorCodeToClient(listSect[idx].sock);
			}
		}
		listSect[idx].nLeft -= *pLength;
		//Send to client
		ret = sendExt(listSect[idx].sock, buff, 3 + *pLength);
	}
	return ret;
}

/*
Function process if msg is TRANSFER_FILE msg
receive a block of file from client
IN int idx : index in the section array and the client array
IN char *buff : buffer use to transfer
IN int buffSize : size of buffer
Return number of byte send to client or SOCKET_ERROR
*/
int processRecvFile(int idx, char *buff, int buffSize) {
	if (listSect[idx].status == UNDEFINED) {
		return sendErrorCodeToClient(listSect[idx].sock);
	}
	int ret;

	short *pLength = (short*)&buff[1];
	if (*pLength == 0) {
		//Recv file completed
		fclose(listSect[idx].pFile);
		listSect[idx].action = SEND_FILE;
		fopen_s(&listSect[idx].pFile, listSect[idx].fileName, "rb");
		if (listSect[idx].pFile == NULL) {
			//Error open file to read
			//printf("Can not open file to read on server\n");
			resetSect(idx);
			//Send error code to client
			return sendErrorCodeToClient(listSect[idx].sock);
		}
		//Init to send file
		fseek(listSect[idx].pFile, 0, SEEK_SET);
		listSect[idx].nLeft = getFileSize(listSect[idx].fileName);
		return 1;
	}
	else {
		//Write buffer to file
		if (listSect[idx].status == ENCODE) {
			encodeBlock(&buff[3], *pLength, listSect[idx].key);
		}
		else {
			decodeBlock(&buff[3], *pLength, listSect[idx].key);
		}

		ret = fwrite(&buff[3], *pLength, 1, listSect[idx].pFile);
		if (ret == 0) {
			//printf("Can not write buffer to file in server\n");
			resetSect(idx);
			return sendErrorCodeToClient(listSect[idx].sock);
		}
	}
	buff[0] = TRANSFER_FILE;
	return sendExt(listSect[idx].sock, buff, 1);
}

/*
Function process with each client
IN int idx : index in the section array and the client array
IN char *buff : buffer use to transfer
IN int buffSize : size of buffer
Return number of byte send to client or SOCKET_ERROR
*/
int processClient(int index, char * buff, int buffSize) {
	int ret;
	//Receive msg from client
	ret = recvExt(listSect[index].sock, buff, buffSize);
	if (ret <= 0) return ret;
	char opCode = buff[0];

	//Analaysis msg and process
	switch (opCode) {

	case ENCODE:
		ret = processEncodeMSG(index, buff, buffSize);
		break;

	case DECODE:
		ret = processDecodeMSG(index, buff, buffSize);
		break;

	case TRANSFER_FILE:
		if (listSect[index].action == RECV_FILE) {
			ret = processRecvFile(index, buff, buffSize);
		}
		if (listSect[index].action == SEND_FILE) {
			ret = processSendFile(index, buff, buffSize);
		}
		break;

	case ERROR_TRANS:

		resetSect(index);
		ret = 1;
		break;
	}
	return ret;
}
