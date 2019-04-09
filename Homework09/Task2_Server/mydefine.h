#pragma once
#ifndef _STDIO_H_
#include <stdio.h>
#endif
#ifndef _WINSOCK2_H_
#include <WinSock2.h>
#endif

#define BLOCK_SIZE 10240
#define BUFF_SIZE 10243 // 10kb + 3 byte
#define FILE_PATCH_SIZE 256
#define ENCODE 0
#define DECODE  1
#define TRANSFER_FILE 2
#define ERROR_TRANS 3
#define READ_FILE_ERROR -10
#define WRITE_FILE_ERROR -20
#define SEND_SUCCESS 10
#define RECV_SUCCESS 20
#define SCRIPT_ERROR -30
#define SEND_FILE 11
#define RECV_FILE 12
#define UNDEFINED -10

