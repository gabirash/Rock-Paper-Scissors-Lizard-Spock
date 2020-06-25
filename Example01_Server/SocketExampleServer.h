/*
 This file was written with help of similar file from the course "Introduction to Systems Programming" at Tel-Aviv
 University, School of Electrical Engineering.
*/

#ifndef SOCKET_EXAMPLE_SERVER_H
#define SOCKET_EXAMPLE_SERVER_H

int MainServer(char* server_port);
static BOOL FindFirstUnusedThreadSlot();
static void CleanupWorkerThreads();
static DWORD exitFromServer(SOCKET *MainSocket);
static DWORD ServiceThread(SOCKET *t_socket);
static int game(char* play1, char* play2);
static int write2leaderboard(char* name, BOOL win);
int remove_leaderboard_and_rename_temp(BOOL first);

#endif // SOCKET_EXAMPLE_SERVER_H
