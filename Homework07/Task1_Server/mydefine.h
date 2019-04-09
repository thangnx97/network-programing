#pragma once
#define BUFF_SIZE 128

#define USERNAME_MSG 10
#define PASSWORD_MSG 20
#define LOGOUT_MSG 30

#define USERNAME_NOTFOUND -11
#define USERNAME_CORRECT 11

#define PASSWORD_INCORRECT -21
#define PASSWORD_CORRECT 21

#define LOGOUT_ERROR -31
#define LOGOUT_SUCCESS 31

#define ACC_LOGINED -41
#define ACC_BLOCKED -40
#define ACCESS_DENIED -50
#define ACTIVE '1' //Account is active
#define BLOCKED '0' //Account is blocked
#define USERNAME_SIZE 32
#define PASSWORD_SIZE 32
//Deffine state 
#define UNDEFINED -1
#define DEFINED 0
#define LOGINED 1
#define MAX_SECTIONS 20
#define ACC_FILE "account.txt"
#define MAX_ACC_COUNT 100

//Define section struct
typedef struct Section {
	SOCKET sock;
	char countIncoPass;
	char status;
	int indexInListAcc;
} Section, *pSection;

typedef struct Account {
	char username[USERNAME_SIZE];
	char password[PASSWORD_SIZE];
	char status;
} Acc, *pAcc;
