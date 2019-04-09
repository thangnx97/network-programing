/*
AddressResolver program
Starting with Winsock and Network programing
This program use gethostbyname(), gethostbyaddr(),... function
to resolve addres in two case:
	+input is an ipv4 address	: list domain address translation 
	+input is a domain name		: list ipv4 address translation

Refference:
	gethostbyadd() function:
	https://msdn.microsoft.com/en-us/library/windows/desktop/ms738521(v=vs.85).aspx
	gethostbyname() function:
	https://msdn.microsoft.com/en-us/library/windows/desktop/ms738524(v=vs.85).aspx
	hostent structure:
	https://msdn.microsoft.com/en-us/library/windows/desktop/ms738552(v=vs.85).aspx

slides: LAP TRINH MANG - Bui Trong Tung, Vien CNTT-TT Dai hoc BKHN
*/

/*
NOTE:
In VS 2015 functions gethostbyname(), gethostbyaddr() is unsafe
Define _WINSOCK_DEPRECATED_NO_WARNINGS before include winsock library to use them
*/


#include <stdio.h>
#include <string.h>
#include <conio.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

/*
Function check if a string is ipv4 address
return 1 if string is a ipv4 address
else return 0
*/
int isIpv4Address(const char *addr) {
	int addrLen = strlen(addr);
	if (addrLen > 15 || addrLen < 7) return 0;
	int dotCount = 0;
	int checkDoubleDot = 1;
	int checkByte = 0;
	for (int i=0; i < addrLen; i++) {
		if (addr[i] == '.') {
			checkDoubleDot++;
			if (checkDoubleDot > 1) return 0;
			checkByte = 0;
			dotCount++;
		}
		else {
			checkDoubleDot = 0;
			if (addr[i]<'0' || addr[i]>'9') return 0;
			checkByte = checkByte * 10 + addr[i] - '0';
			if (checkByte > 255) return 0;
		}
	}
	if (dotCount !=3 || addr[addrLen - 1] == '.') return 0;
	return 1;
}

/*
This function show list of domain nam
from a ipv4 address input
using gethostbyaddr() function
Return 0 if resolver success
else return 1
*/
int resolveIpAddress(const char * ipAddr) {
	in_addr addr;
	addr.s_addr = inet_addr(ipAddr);
	hostent *host = gethostbyaddr((char*)&addr, sizeof(ipAddr), AF_INET);
	if (host == NULL) return 1;
	
	printf("List of domain:\n");
	printf("%s\n", host->h_name);
	char **moreDomainName = host->h_aliases;
	int i = 0;
	while (*(moreDomainName + i) != NULL) {
		printf("%s\n", *(moreDomainName + i));
		i++;
	}
	return 0;
}

/*
Show list of ipv4 address
from a domain name input
Function return 0 if resolve suscess
else return 1
*/
int resolveDomainAddress(const char *domainName) {
	hostent *host = gethostbyname(domainName);
	if (host == NULL) return 1;
	
	printf("List of ip:\n");
	char **listIp = host->h_addr_list;
	int i = 0;
	in_addr addr;
	while (*(listIp + i) != NULL) {
		addr.s_addr = *(u_long *)*(listIp+i);
		printf("%s\n", inet_ntoa(addr));
		i++;
	}
	return 0;
}

int main() {
	/*
	Init winsock
	used winsock version: 2.2
	*/
	WSADATA wsaData;
	int startUpErr = WSAStartup(
		MAKEWORD(2, 2),
		&wsaData
	);

	/*
	Return 1 and exit if init winsock fail
	*/
	if (startUpErr != 0) {
		printf("Can not start up winsock");
		system("pause");
		return 1;
	}

	/*
	Init address buffer
	array addrBuff use to contain the ipv4 address or domain name
	*/
	char addrBuff[256];
	printf("Enter domain or ip address: ");
	gets_s(addrBuff, 255);
	int ret;

	/*
	Loop to input and resolve address 
	until user enter an null string
	*/
	while (strlen(addrBuff)) {		
		if (isIpv4Address(addrBuff)) {
			ret = resolveIpAddress(addrBuff);
			if (ret) printf("Invalid IP address\n");
		}
		else {
			ret = resolveDomainAddress(addrBuff);
			if (ret) printf("Not found infomation\n");
		}
		printf("Enter domain or ip address: ");
		gets_s(addrBuff, 255);
	}

	/*
	Terminates use of winsock
	*/
	WSACleanup();
	
	/*
	User press any key to exit program
	*/
	system("pause");
	return 0;
}