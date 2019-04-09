#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
/*
Define _WINSOCK_DEPRECATED_NO_WARNINGS to use some function
that not safe in VS2015
*/
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <ws2tcpip.h>
#include <winsock2.h>
#define MAX_PORT 65535 // (2^16) -1
#define BUFF_SIZE 256

#pragma comment(lib, "Ws2_32.lib")

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
	int i = 0, portNum =0;
	while (i < len) {
		if (portAddr[i]<'0' || portAddr[i]>'9') return -1;
		portNum = portNum * 10 + (portAddr[i] - '0');
		i++;
	}
	return (portNum<= MAX_PORT) ? portNum: -1;
}

/*
Function return sum of all number in a string
If string contain any charecter is not number: function return -1
*/
int sumNumberInString(const char * string) {
	int sum = 0, len = strlen(string), i;
	for (i = 0; i < len; i++) {
		if (string[i]<'0' || string[i]>'9') return -1;
		sum += (string[i] - '0');
	}
	return sum;
}

int main(int argc, char ** argv) {
	//Pre process argument
	if (argc != 2) {
		printf("Arguments not found\n");
		system("pause");
		return 1;
	}

	int port = isPortNum(argv[1]);
	if (port == -1) {
		printf("Port input not found\n");
		system("pausse");
		return 1;
	}

	/*
	Init winsock
	Use version 2.2
	*/
	WSAData wsaData;
	int ret;
	ret = WSAStartup(
		MAKEWORD(2, 2),
		&wsaData
	);

	if (ret != 0) {
		printf("Canot init winsock!");
		system("pause");
		return 1;
	}

	//Init socket
	SOCKET sever = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sever == SOCKET_ERROR) {
		printf("Error code: %d", WSAGetLastError());
		system("pause");
		return 1;
	}

	//Init sever address usse INADDR_ANY argument...
	sockaddr_in severAddr;
	severAddr.sin_family = AF_INET;
	severAddr.sin_addr.s_addr = htonl(ADDR_ANY);
	severAddr.sin_port = htons(port);

	//Bind address to socket
	ret = bind(sever, (const sockaddr*) &severAddr, sizeof(severAddr));
	if (ret != 0) {
		printf("Can not bind address!\n");
		system("pause");
		return 1;
	}
	printf("Sever started at [%s:%u]\n", 
		inet_ntoa(severAddr.sin_addr),
		ntohs(severAddr.sin_port));

	//Init to transfer
	sockaddr_in clientAddr;
	char buff[BUFF_SIZE];
	int clientAddrLen = sizeof(clientAddr), result;
	
	//Infinite lopp
	while (1) {
		//Receiver messange from client
		ret = recvfrom(sever, buff, BUFF_SIZE, 0,
			(sockaddr *)&clientAddr, &clientAddrLen);
		if (ret == SOCKET_ERROR) {
			printf("Can not receive message\n");
		}
		else {
			buff[ret] = '\0';
			printf("Recive from [%s:%u]: %s\n",
				inet_ntoa(clientAddr.sin_addr), 
				ntohs(clientAddr.sin_port), buff);
			result = sumNumberInString(buff);
			//Set buff result to send back to client
			if (result == -1) {

				if (strlen(buff) == 1 && (buff[0] == 'q' || buff[0] == 'Q')) {
					/*
					Do if client send "Q" or "q" string, 
					echo the string to cliend handle terminated...
					*/
					strcpy_s(buff, "Q");
				}
				else {
					strcpy_s(buff, "Error");
				}
			}
			else {
				//Do if string have sum of the number charecter
				_itoa_s(result, buff, 10);
			}
			//Send solution to client
			ret = sendto(sever, buff, strlen(buff), 0, 
				(const sockaddr *)&clientAddr, clientAddrLen);
			if (ret == SOCKET_ERROR) {
				printf("Can not send to client\n");
			}
			else {
				printf("Send to client [%s:%u]: %s\n",
					inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), buff);
			}
		}
	}
	
	//End of ussing socket sever
	closesocket(sever);
	//End of ussing winsock
	WSACleanup();
	return 0;
}