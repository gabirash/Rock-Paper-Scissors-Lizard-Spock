#pragma once

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include "SocketExampleClient.h"

#define MAX_ARGUMENTS 3
#define ERROR_CODE -1
#define SUCCESS_CODE 0
#define MAX_CSV_LINE_LEN 128 //constant size (name=20 + 3*2 tabs + w+l+w/l=5 + spare)
#define MAX_SERVER_IP 16
#define MAX_SERVER_PORT 5
#define MAX_USERNAME_LEN 21
#define SERVER_EXIT_ERROR (-5)
