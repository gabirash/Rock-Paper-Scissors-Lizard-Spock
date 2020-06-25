/*
 This file was written with help of similar file from the course "Introduction to Systems Programming" at Tel-Aviv
 University, School of Electrical Engineering.
*/

#ifndef SOCKET_EXAMPLE_CLIENT_H
#define SOCKET_EXAMPLE_CLIENT_H

static DWORD RecvDataThread(void);
static DWORD SendDataThread(void);
int MainClient(char *server_ip, char* server_port, char* username);

#endif // SOCKET_EXAMPLE_CLIENT_H
