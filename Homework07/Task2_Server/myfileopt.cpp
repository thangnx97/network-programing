#include "stdafx.h"

#ifndef _STDIO_H_
#include <stdio.h>
#endif

#ifndef _STRING_H_
#include <string.h>
#endif

#define FILE_NOT_EXISTS -1
#define FILE_EXISTED 1

/*
Function check file exist
Open file with "r" option
If file exist return 1
Else return 0
*/
int fileExist(char * filePatch) {
	FILE *file;
	fopen_s(&file, filePatch, "r");
	if (file == NULL) return FILE_NOT_EXISTS;
	fclose(file);
	return FILE_EXISTED;
}

/*Function get file size
From file patch input
Return file size by byte
*/
_int64 getFileSize(char * filePatch) {
	FILE *file;
	fopen_s(&file, filePatch, "r");
	if (file == NULL) return FILE_NOT_EXISTS;
	_fseeki64(file, 0L, SEEK_END);
	_int64 fileSize = _ftelli64(file);
	fclose(file);
	return fileSize;
}

/*Function split file name from file patch
filePatch: input string contain file patch
fileName: ouput string contain file name
Function return length of file name
*/
int splitFileName(char * filePatch, char * fileName) {
	int i = 0;
	int index = 0;
	while (i < strlen(filePatch)) {
		if (filePatch[i] == '\\') index = i;
		i++;
	}
	for (i = 0; i < strlen(filePatch) - index; i++) {
		fileName[i] = filePatch[index + i + 1];
	}
	fileName[i] = '\0';
	return strlen(fileName);
}
