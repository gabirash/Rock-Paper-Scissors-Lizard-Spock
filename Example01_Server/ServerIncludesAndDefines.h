#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <stdlib.h> //for the rand function
#include <Windows.h>
#include "SocketExampleServer.h"

#define MAX_SERVER_PORT 5
#define NUM_OF_WORKER_THREADS 2
#define ERROR_CODE -1
#define SUCCESS_CODE 0
#define MAX_USERNAME_LEN 21
#define EXIT_LEN 7
#define SEND_STR_SIZE 128 //Maximal constant size with spare (malloc not neccesary with proper input)
#define MAX_LINE_IN_FILE_LEN 128 //Maximal constan size with spare (malloc not neccesary with proper input)
#define MAX_CLIENT_MSG_LEN 64 
#define MAX_MSG_TO_SEND 256
#define MAX_LEADER_ARG 3000
#define MAX_CPY_LINE 1024