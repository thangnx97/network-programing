#ifndef _WINSOCK2_H_
#include <winsock2.h>
#endif

#ifndef _WS2TCPIP_H_
#include <ws2tcpip.h>
#endif

int sendExt(SOCKET connSock, char * buff, int buffSize);

int recvExt(SOCKET connSock, char * buff, int buffSize);

int isPortNum(char *portAddr);

int isIpv4Address(char *addr);
