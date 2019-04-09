#ifndef _WINSOCK2_H_
#include <winsock2.h>
#endif

#ifndef _STDIO_H_
#include <stdio.h>
#endif

#ifndef _WS2TCPIP_H_
#include <ws2tcpip.h>
#endif

#ifndef MAX_PORT
#define MAX_PORT 65535 // (2^16) -1
#endif

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
	
	if (dotCount != 3 || addr[len] == '.') return 0;
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

//Recv border function for client
int recvExt(SOCKET connSock, char * buff, int buffSize) {
	//Not safe
	int ret = recv(connSock, buff, buffSize, 0);
	if (0<ret && ret < 10) {
		printf("Receive: %d byte\n", ret);
		for (int i = 0; i < ret; i++) {
			printf("%d ", buff[i]);
		}
		printf("\n");
	}
	return ret;
}

//Send border function for client
int sendExt(SOCKET connSock, char *buff, int dataLen) {
	printf("Send to sever: %d byte\n", dataLen);
	if (dataLen <= 3) {
		printf("%d %d %d\n", buff[0], buff[1], buff[2]);
	}
	return send(connSock, buff, dataLen, 0);
}

