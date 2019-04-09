#ifndef _STDIO_H_
#include <stdio.h>
#endif

#define FILE_NOT_EXISTS -1
#define FILE_EXISTED 1

int fileExist(char* filePatch);

_int64 getFileSize(char* filePatch);

int splitFileName(char* filePatch, char* fileName);

