#ifndef _WINSOCK2_H_
#include <winsock2.h>
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

//Function receive extend
//Return...
int recvExt(SOCKET connSock, char * buff, int buffSize) {
	int dataLen;
	int ret;
	//Receive 4 byte of data length
	ret = recv(connSock, (char*)&dataLen, sizeof(int), MSG_WAITALL);
	if (ret == SOCKET_ERROR) return SOCKET_ERROR;
	//Receive data
	if (dataLen <= buffSize) {
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

