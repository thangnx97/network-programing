#include <stdio.h>
#include <stdlib.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <ws2tcpip.h>
#define MAX_PORT 65535 // (2^16) -1
#define BUFF_SIZE 256

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
		printf("Init  winsock fail\n");
		system("pause");
		return 1;
	}
	printf("Client started!\n");

	//Init socket
	SOCKET client = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (client == SOCKET_ERROR) {
		printf("Init socket fail\n");
		system("pause");
		return 1;
	}
	
	//Set receive time out option = 10s
	int tv = 10000;
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(int));

	//Init sever address by input arguments
	sockaddr_in severAddr;
	severAddr.sin_family = AF_INET;
	severAddr.sin_port = htons(severPort);
	severAddr.sin_addr.s_addr = inet_addr(severIp);

	//Init to transfer
	char buff[BUFF_SIZE];
	int ret, severAddrLen = sizeof(severAddr);
	printf("Connect to sever [%s:%d]:\n", 
		inet_ntoa(severAddr.sin_addr), ntohs(severAddr.sin_port));
	printf("Send to sever: ");
	gets_s(buff, BUFF_SIZE);
	//Loop until usser send "Q" or "q" string
	while (strcmp(buff, "Q") != 0) {
		//Send message to sever
		ret = sendto(client, buff, strlen(buff), 0, (const sockaddr*)&severAddr, severAddrLen);
		if (ret == SOCKET_ERROR) {
			printf("Error! Can not send message.\n");
		}
		else {
			//Receive result message from sever
			ret = recvfrom(client, buff, BUFF_SIZE, 0, (sockaddr*)&severAddr, &severAddrLen);
			if (ret == SOCKET_ERROR) {
				if (WSAGetLastError() == WSAETIMEDOUT) {
					printf("Time out.\n");
				}
				else {
					printf("Can not receive result.\n");
				}
			}
			else {
				if (strlen(buff) > 0) {
					buff[ret] = 0;
					printf("%s\n", buff);
				}
			}
		}
		printf("Send to sever: ");
		gets_s(buff, BUFF_SIZE);
		_strupr_s(buff);
	}

	//End of ussing socket
	closesocket(client);
	//End of ussing winsock
	WSACleanup();

	return 0;
}