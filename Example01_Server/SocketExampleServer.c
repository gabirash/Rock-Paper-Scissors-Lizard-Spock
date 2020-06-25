/*
 This file was written with help of similar file from the course "Introduction to Systems Programming" at Tel-Aviv
 University, School of Electrical Engineering.
 */

#include "ServerIncludesAndDefines.h"
#include "SocketExampleShared.h"
#include "SocketSendRecvTools.h"

/*Global variables*/
HANDLE ThreadHandles[NUM_OF_WORKER_THREADS]; //Initialization is done in MainServer!
SOCKET ThreadInputs[NUM_OF_WORKER_THREADS];
static HANDLE write2file_mutex_handle = NULL; //for gamesession mutex
HANDLE gamesession_semaphore = NULL; //semaphore for option 1 in the main menu 
HANDLE leaderboard_semaphore = NULL; //for semaphores
HANDLE semaphores_arr[2] = { NULL };
int Ind;
char first_user[MAX_USERNAME_LEN] = "";
char second_user[MAX_USERNAME_LEN] = "";
BOOL someone_inside = FALSE, lets_play = FALSE, an_error_occured_remove_LB = FALSE;
FILE* fp_leaderboard = NULL;
char computer_move[MAX_PLAY_MOVE_SIZE] = "";

/*
Description: The fuction that is being called from the server_main. It binds the socket and listens on it.
			 It also generates the leader board. Finally cleanups being made.
Parameters:  char* server_port - the port (server).
Returns:	 exit program with 0 if everything went ok, -1 else.
*/

int MainServer(char* server_port)
{
	SOCKET MainSocket = INVALID_SOCKET;
	unsigned long Address=-1;
	SOCKADDR_IN service;
	int bindRes=-1;
	int ListenRes=-1;
	BOOL ret_val = FALSE; //for mutex
	HANDLE ThreadExitServerHandle = NULL;
	static HANDLE write2leaderboard_file_mutex_handle = NULL; //for mutex
	BOOL an_error_occured_main = FALSE;

	//Initialize Winsock.
	WSADATA wsaData;
	int StartupRes = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (StartupRes != NO_ERROR)
	{
		// Tell the user that we could not find a usable WinSock DLL.
		printf("error %ld at WSAStartup( ), ending program.\n", WSAGetLastError());
		an_error_occured_main = TRUE;                                  
		return ERROR_CODE;
	}

	/* Create the mutex that will be used to synchronize access writing into the GameSession file */
	write2file_mutex_handle = CreateMutex(
		NULL,	/* default security attributes */
		FALSE,	/* initially not owned */
		NULL);	/* unnamed mutex */
	if (NULL == write2file_mutex_handle)
	{
		an_error_occured_main = TRUE;
		printf("An error has occured while creating the mutex, Last Error = 0x%x\n!", GetLastError());
		goto cleanup_wsaup;
	}

	/* Create the mutex that will be used to synchronize access writing into the Leadeboard file */
	write2leaderboard_file_mutex_handle = CreateMutex(
		NULL,	/* default security attributes */
		FALSE,	/* initially not owned */
		NULL);	/* unnamed mutex */
	if (NULL == write2leaderboard_file_mutex_handle)
	{
		an_error_occured_main = TRUE;
		printf("An error has occured while creating the mutex, Last Error = 0x%x\n!", GetLastError());
		goto close_mutexs;
	}

	//create semaphores
	gamesession_semaphore = CreateSemaphore(
		NULL,					/* Default security attributes */
		0,						/* Initial Count - all slots are empty */  
		2,						/* Maximum Count */ 
		NULL);					/* named */

	if (gamesession_semaphore == NULL)
	{
		an_error_occured_main = TRUE;
		printf("An error has occured while creating the semaphore, Last Error = 0x%x\n!", GetLastError());
		goto close_semaphores;
	}

	leaderboard_semaphore = CreateSemaphore(
		NULL,					/* Default security attributes */
		2,						/* Initial Count - all slots are empty */
		2,						/* Maximum Count */
		NULL);					/* named */

	if (leaderboard_semaphore == NULL)
	{
		an_error_occured_main = TRUE;
		printf("An error has occured while creating the semaphore, Last Error = 0x%x\n!", GetLastError());
		goto close_semaphores;
	}

	for (int i = 0; i < 2; i++)
	{
		semaphores_arr[i] = CreateSemaphore(
			NULL,					/* Default security attributes */
			0,						/* Initial Count - all slots are empty */
			2,						/* Maximum Count */
			NULL);					/* named */

		if (semaphores_arr[i] == NULL)
		{
			an_error_occured_main = TRUE;
			printf("An error has occured while creating the semaphore, Last Error = 0x%x\n!", GetLastError());
			goto close_semaphores;
		}
	}

	//create Leaderboard.csv file just if it's not existed already
	if (NULL == (fp_leaderboard = fopen("Leaderboard.csv", "r")))
	{
		if (NULL == (fp_leaderboard = fopen("Leaderboard.csv", "a")))
		{
			an_error_occured_main = TRUE;
			printf("Error when open file");
			goto close_files;
		}
		fprintf(fp_leaderboard, "Name,Won,Lost,W/L Ratio\n");
	}
	fclose(fp_leaderboard);

	if (NULL == (fp_leaderboard = fopen("Leaderboard.csv", "a")))
	{
		an_error_occured_main = TRUE;
		printf("Error when open file");
		goto close_files;
	}

	fclose(fp_leaderboard);

	/* The WinSock DLL is acceptable. Proceed. */

	// Create a socket.    
	MainSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (MainSocket == INVALID_SOCKET)
	{
		printf("Error at socket( ): %ld\n", WSAGetLastError());
		an_error_occured_main = TRUE;
		goto server_cleanup_1;
	}

	// Bind the socket.
	/*
		For a server to accept client connections, it must be bound to a network address within the system.
		The following code demonstrates how to bind a socket that has already been created to an IP address
		and port.
		Client applications use the IP address and port to connect to the host network.
		The sockaddr structure holds information regarding the address family, IP address, and port number.
		sockaddr_in is a subset of sockaddr and is used for IP version 4 applications.
   */
   // Create a sockaddr_in object and set its values.
   // Declare variables

	Address = inet_addr(SERVER_ADDRESS_STR);
	if (Address == INADDR_NONE)
	{
		printf("The string \"%s\" cannot be converted into an ip address. ending program.\n", SERVER_ADDRESS_STR);
		an_error_occured_main = TRUE;
		goto server_cleanup_2;
	}

	service.sin_family = AF_INET;
	service.sin_addr.s_addr = Address;
	service.sin_port = htons(atoi(server_port)); //The htons function converts a u_short from host to TCP/IP network byte order 
									   //( which is big-endian ).
	/*
		The three lines following the declaration of sockaddr_in service are used to set up
		the sockaddr structure:
		AF_INET is the Internet address family.
		"127.0.0.1" is the local IP address to which the socket will be bound.
		2345 is the port number to which the socket will be bound.
	*/

	// Call the bind function, passing the created socket and the sockaddr_in structure as parameters. 
	// Check for general errors.
	bindRes = bind(MainSocket, (SOCKADDR*)&service, sizeof(service));
	if (bindRes == SOCKET_ERROR)
	{
		printf("bind( ) failed with error %ld. Ending program\n", WSAGetLastError());
		an_error_occured_main = TRUE;
		goto server_cleanup_2;
	}

	// Listen on the Socket.
	ListenRes = listen(MainSocket, SOMAXCONN);
	if (ListenRes == SOCKET_ERROR)
	{
		printf("Failed listening on socket, error %ld.\n", WSAGetLastError());
		an_error_occured_main = TRUE;
		goto server_cleanup_2;
	}

	// Initialize all thread handles to NULL, to mark that they have not been initialized
	for (Ind = 0; Ind < NUM_OF_WORKER_THREADS; Ind++)
		ThreadHandles[Ind] = NULL;

	printf("Waiting for a client to connect...\n");

	//always wait for exit to come in server
	ThreadExitServerHandle = CreateThread(
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)exitFromServer,
		&MainSocket,
		0,
		NULL
	);

	if (NULL == ThreadExitServerHandle)
	{
		printf("Couldn't create server exit thread\n");
		an_error_occured_main = TRUE;
		goto server_cleanup_2;
	}

	while (1) //endless clients, just 2 can connect
	{
		TransferResult_t SendRes;
		TransferResult_t RecvRes;
		
		SOCKET AcceptSocket = accept(MainSocket, NULL, NULL);
		if (AcceptSocket == INVALID_SOCKET)
		{
			printf("Accepting connection with client failed, error %ld\n", WSAGetLastError());
			goto server_cleanup_3;
		}

		an_error_occured_main = FindFirstUnusedThreadSlot();

		if (Ind == NUM_OF_WORKER_THREADS) //no slot is available
		{
			printf("No slots available for client, dropping the connection.\n");
			SendRes = SendString("SERVICE_DENIED:2CLIENTS", AcceptSocket);//send message to client
			if (SendRes == TRNS_FAILED)
			{
				printf("Service socket error while writing, closing thread.\n");
				closesocket(AcceptSocket); //Closing the socket, dropping the connection.
				an_error_occured_main = TRUE;
				goto server_cleanup_3;
			}

		}
		else
		{
			ThreadInputs[Ind] = AcceptSocket; // shallow copy: don't close 
											  // AcceptSocket, instead close 
											  // ThreadInputs[Ind] when the
											  // time comes.

			ThreadHandles[Ind] = CreateThread(
				NULL,
				0,
				(LPTHREAD_START_ROUTINE)ServiceThread,
				&(ThreadInputs[Ind]),
				0,
				NULL
			);
			if (NULL == ThreadHandles[Ind])
			{
				printf("Couldn't create thread\n");
				an_error_occured_main = TRUE;
			}
		}

	} 

server_cleanup_3:
	CleanupWorkerThreads();
	ret_val = CloseHandle(ThreadExitServerHandle);
	if (FALSE == ret_val)
	{
		printf("Error when closing thread: %d\n", GetLastError());
		an_error_occured_main = TRUE;
	}
	ThreadExitServerHandle = NULL;

server_cleanup_2:
	if (closesocket(MainSocket) == SOCKET_ERROR)
	{
		printf("Failed to close MainSocket, error %ld. Ending program\n", WSAGetLastError());
	}

server_cleanup_1:
	if (WSACleanup() == SOCKET_ERROR)
	{
		printf("Failed to close Winsocket, error %ld. Ending program.\n", WSAGetLastError());
		an_error_occured_main = TRUE;
	}

close_files:
	fclose(fp_leaderboard);

close_semaphores:
	ret_val = CloseHandle(gamesession_semaphore);
	if (FALSE == ret_val)
	{
		printf("Error when closing semaphore handle: %d\n", GetLastError());
		an_error_occured_main = TRUE;
	}

	ret_val = CloseHandle(leaderboard_semaphore);
	if (FALSE == ret_val)
	{
		printf("Error when closing semaphore handle: %d\n", GetLastError());
		an_error_occured_main = TRUE;
	}

	for (int i = 0; i < 2; i++)
	{
		ret_val = CloseHandle(semaphores_arr[i]);
		if (FALSE == ret_val)
		{
			printf("Error when closing semaphore handle: %d\n", GetLastError());
			an_error_occured_main = TRUE;
		}
	}

close_mutexs:
	ret_val = CloseHandle(write2file_mutex_handle);
	if (FALSE == ret_val)
	{
		printf("Error when closing mutex handle: %d\n", GetLastError());
		an_error_occured_main = TRUE;
	}

	ret_val = CloseHandle(write2leaderboard_file_mutex_handle);
	if (FALSE == ret_val)
	{
		printf("Error when closing mutex handle: %d\n", GetLastError());
		an_error_occured_main = TRUE;
	}

cleanup_wsaup:
	WSACleanup();

	if (an_error_occured_main)
		if (an_error_occured_remove_LB)
		return ERROR_CODE;
	return SUCCESS_CODE;
}

/*
Description: This is the function that finds the first unused slot, by updating Ind.
Parameters:  None.
Returns:	 None.
*/

static BOOL FindFirstUnusedThreadSlot()
{
	BOOL an_error_occured_to_main = FALSE;
	for (Ind = 0; Ind < NUM_OF_WORKER_THREADS; Ind++)
	{
		if (ThreadHandles[Ind] == NULL)
			break;
		else
		{
			// poll to check if thread finished running:
			DWORD Res = WaitForSingleObject(ThreadHandles[Ind], 0);

			if (Res == WAIT_OBJECT_0) // this thread finished running
			{
				BOOL ret_val_loc = CloseHandle(ThreadHandles[Ind]);
				if (FALSE == ret_val_loc)
				{
					printf("Error when closing thread: %d\n", GetLastError());
					an_error_occured_to_main = TRUE;
				}
				ThreadHandles[Ind] = NULL;
				break;
			}
		}
	}
	return an_error_occured_to_main;
}

/*
Description: This is the function that cleans the WorkerThreads, by updated Ind.
Parameters:  None.
Returns:	 None.
*/

static void CleanupWorkerThreads()
{
	int Ind;
	BOOL an_error_occured_cu = FALSE;

	for (Ind = 0; Ind < NUM_OF_WORKER_THREADS; Ind++)
	{
		if (ThreadHandles[Ind] != NULL)
		{
			// poll to check if thread finished running:
			DWORD Res = WaitForSingleObject(ThreadHandles[Ind], INFINITE);

			if (Res == WAIT_OBJECT_0)
			{
				closesocket(ThreadInputs[Ind]);
				CloseHandle(ThreadHandles[Ind]);
				ThreadHandles[Ind] = NULL;
				break;
			}
			else
			{
				printf("Waiting for thread failed. Ending program\n");
				an_error_occured_cu = TRUE;
				return;
			}
		}
	}
}

/*
Description: Function for exiting from the server, by updated Ind.
Parameters:  SOCKET *MainSocket, the main socket.
Returns:	 None.
*/

static DWORD exitFromServer(SOCKET *MainSocket)
{
	char exitStr[EXIT_LEN]="";
	while (1)
	{
		gets_s(exitStr, sizeof(exitStr));
		if (STRINGS_ARE_EQUAL(exitStr, "exit")) {
			for (int i = 0; i < NUM_OF_WORKER_THREADS; i++)
			{
				if (ThreadInputs[i] != NULL)
					closesocket(ThreadInputs[i]);
			}
			//WSACleanup();
			closesocket(*MainSocket);
			//an Event is needed to be used in order to work perfectly.
		}
	}
}

/*
Description: Service thread is the thread that opens for each successful client connection and "talks" to the client.
Parameters:  SOCKET *t_socket, the Accept socket.
Returns:	 1 if succeeded, 0 else.
*/

static DWORD ServiceThread(SOCKET* t_socket)
{
	BOOL Done = FALSE, an_error = FALSE, server_play = FALSE, client_play = FALSE;
	DWORD wait_res;
	TransferResult_t SendRes;
	TransferResult_t RecvRes;
	FILE* fp_GameSession = NULL;
	char username[MAX_USERNAME_LEN], other_username[MAX_USERNAME_LEN], user_move[MAX_PLAY_MOVE_SIZE]; //maximal username length and maximal possible move
	int mess_type_capacity = MAX_CLIENT_MSG_LEN, msg_ts_capacity = MAX_MSG_TO_SEND;
	//char *message_type=NULL; 
	char message_type[20];
	char SendStr[SEND_STR_SIZE];
	const char* play_move[] = { "ROCK", "PAPER", "SCISSORS", "LIZARD", "SPOCK" };
	char* AcceptedStr = NULL;
	char* token = NULL;
	int my_index = -1;

	//wait for CLIENT_REQUSET
	RecvRes = ReceiveString(&AcceptedStr, *t_socket);
	if (RecvRes == TRNS_FAILED)
	{
		printf("Service socket error while reading, closing thread.\n");
		goto socket_error;
	}

	else if (RecvRes == TRNS_DISCONNECTED)
	{
		printf("Connection closed while reading, closing thread.\n");
		goto socket_error;
	}

	token = strtok(AcceptedStr, ":"); //extract message type
	//mess_type_capacity = MAX_CLIENT_MSG_LEN + strlen(token);
	/*message_type = (char*)malloc(mess_type_capacity);
	if (NULL == message_type) {
		printf("Memory allocation has failed!");
		an_error = TRUE;
		goto socket_error;
	}*/
	strcpy(message_type, token);
	token = strtok(NULL, ":"); //extract username
	strcpy(username, token);

	//create users names
	if (!strcmp(first_user, ""))
		strcpy(first_user, username);
	else
		strcpy(second_user, username);

	//printf("first user is: %s\nSecond user is: %s\n", first_user, second_user);

	if (STRINGS_ARE_EQUAL(message_type, "CLIENT_REQUEST"))
	{
		strcpy(SendStr, "SERVER_APPROVED");
		SendRes = SendString(SendStr, *t_socket);//send message to client

		if (SendRes == TRNS_FAILED)
		{
			printf("Service socket error while writing, closing thread.\n");
			goto socket_error;
		}
	}

	//show the Main menu to client
	strcpy(SendStr, "SERVER_MAIN_MENU");
	SendRes = SendString(SendStr, *t_socket);
	if (SendRes == TRNS_FAILED)
	{
		printf("Service socket error while writing, closing thread.\n");
		goto socket_error;
	}

	while (!Done)
	{
		BOOL an_error_occured_service = FALSE;
		int rand_index = -1, winner = -1;
		AcceptedStr = NULL;
		char winner_string[MAX_USERNAME_LEN] = "", other_move[MAX_PLAY_MOVE_SIZE] = "";
		char msg_to_send[MAX_MSG_TO_SEND] = "";

		//for mutex
		DWORD wait_code;
		BOOL  ret_val;

		//char message_type[50] = "", arg1[20] = "";

		RecvRes = ReceiveString(&AcceptedStr, *t_socket);
		if (RecvRes == TRNS_FAILED)
		{
			printf("Service socket error while reading, closing thread.\n");
			//closesocket(*t_socket);
			goto socket_error;
		}
		else if (RecvRes == TRNS_DISCONNECTED)
		{
			printf("Connection closed while reading, closing thread.\n");
			//closesocket(*t_socket);
			goto socket_error;
		}

		//extract message type and paramters if there are
		token = strtok(AcceptedStr, ":"); //extract message type
		if(token != NULL) strcpy(message_type, token);

		if (STRINGS_ARE_EQUAL(message_type, "CLIENT_REQUEST"))
		{
			token = strtok(NULL, ":"); //extract other username
			strcpy(other_username, token);
		}

		if (STRINGS_ARE_EQUAL(message_type, "CLIENT_PLAYER_MOVE"))
		{
			token = strtok(NULL, ":"); //extract other username
			strcpy(user_move, token);
		}

		if (STRINGS_ARE_EQUAL(AcceptedStr, "CLIENT_MAIN_MENU"))
		{
			server_play = FALSE;
			client_play = FALSE;
			strcpy(SendStr, "SERVER_MAIN_MENU");
			SendRes = SendString(SendStr, *t_socket);
			if (SendRes == TRNS_FAILED)
			{
				printf("Service socket error while writing, closing thread.\n");
				//closesocket(*t_socket);
				goto socket_error;
			}
		}

		else if (STRINGS_ARE_EQUAL(AcceptedStr, "CLIENT_CPU") || ((STRINGS_ARE_EQUAL(AcceptedStr, "CLIENT_REPLAY")) && server_play))
		{
			server_play = TRUE;
			rand_index = rand() % 5; //gives a random number 0-4 
			strcpy(computer_move, play_move[rand_index]);
			SendRes = SendString("SERVER_PLAYER_MOVE_REQUEST", *t_socket); //send request for a move
			if (SendRes == TRNS_FAILED)
			{
				printf("Service socket error while writing, closing thread.\n");
				goto socket_error;
			}
		}

		else if (STRINGS_ARE_EQUAL(AcceptedStr, "CLIENT_VERSUS"))//option number 1 in the main menu 
		{
			client_play = TRUE;
			if (someone_inside == FALSE) //just one client choose this option
			{
				my_index = 0; //I'm the first client!
				someone_inside = TRUE;
				//need to wait until another user wants to play
				wait_res = WaitForSingleObject(gamesession_semaphore, 15000); //waits 15 seconds for aother user to choose 1

				if (wait_res == WAIT_FAILED) //error
				{
					goto socket_error;
				}

				else if (wait_res == WAIT_TIMEOUT) //30 seconds passsed and there's is no opponent! show main menu
				{
					someone_inside = FALSE;
					SendRes = SendString("SERVER_NO_OPPONENTS", *t_socket);

					if (SendRes == TRNS_FAILED)
					{
						printf("Service socket error while writing, closing thread.\n");
						goto socket_error;
					}

					SendRes = SendString("SERVER_MAIN_MENU", *t_socket);

					if (SendRes == TRNS_FAILED)
					{
						printf("Service socket error while writing, closing thread.\n");
						goto socket_error;
					}
				}
			}

			else //2 clients choose option 1
			{
				my_index = 1; //I'm the second client
				//release other client who stucks in semaphore
				ret_val = ReleaseSemaphore(
					gamesession_semaphore,
					1, 		/* Signal that exactly one cell was emptied */
					NULL);
				if (ret_val == FALSE)
				{
					goto socket_error;
				}

				lets_play = TRUE;
			}

			someone_inside = FALSE; //update for next game!

			/****we have 2 clients who wants to play against each other******/
			if (lets_play)
			{
				//find other username
				if (!strcmp(username, first_user))
					strcpy(other_username, second_user);
				else
					strcpy(other_username, first_user);

				//send both players server_invite message
				sprintf(SendStr, "SERVER_INVITE:%s", other_username);
				SendRes = SendString(SendStr, *t_socket); //send to client main menu
				if (SendRes == TRNS_FAILED)
				{
					printf("Service socket error while writing, closing thread.\n");
					goto socket_error;
				}

				//ask both players to choose a move
				SendRes = SendString("SERVER_PLAYER_MOVE_REQUEST", *t_socket);
				if (SendRes == TRNS_FAILED)
				{
					printf("Service socket error while writing, closing thread.\n");
					goto socket_error;
				}

				lets_play = FALSE;
			}
		}


		else if (STRINGS_ARE_EQUAL(message_type, "CLIENT_LEADERBOARD") || STRINGS_ARE_EQUAL(message_type, "CLIENT_REFRESH"))
		{
			char line[MAX_LINE_IN_FILE_LEN] = "", line_cpy[MAX_LINE_IN_FILE_LEN] = "", leaderboard_argument_line[3000] = "", line2send_cpy[1024];
			char* line2send = NULL;
		//	int last_size = 0;
			FILE* fp_temp = NULL;

			//first, catch mutex
			wait_code = WaitForSingleObject(write2file_mutex_handle, INFINITE);
			if (WAIT_OBJECT_0 != wait_code)
			{
				an_error_occured_service = TRUE;
				printf("Error when waiting for mutex\n");
				goto socket_error;
			}

			//now, catch semaphore
			wait_code = WaitForSingleObject(leaderboard_semaphore, INFINITE);
			if (wait_code != WAIT_OBJECT_0)
			{
				an_error_occured_service = TRUE;
				printf("Error when waiting for semaphore\n");
			}

			/*We have mutex and semaphore*/

			//open file for reading
			if (NULL == (fp_leaderboard = fopen("Leaderboard.csv", "r")))
			{
				an_error_occured_service = TRUE;
				printf("Error when open file");
			}

			//release mutex, still holding semaphore!
			ret_val = ReleaseMutex(write2file_mutex_handle);
			if (FALSE == ret_val)
			{
				an_error_occured_service = TRUE;
				printf("Error when releasing\n");
			}

			fgets(line, sizeof(line), fp_leaderboard); //extract first line
			strcpy(leaderboard_argument_line, "SERVER_LEADERBOARD:");//concat messege type
			strcat(leaderboard_argument_line, line); //concat first line with all the headers	
			while (fgets(line, sizeof(line), fp_leaderboard) != NULL) //while loop over all the file
				strcat(leaderboard_argument_line, line);

			SendRes = SendString(leaderboard_argument_line, *t_socket);//send message to client
			if (SendRes == TRNS_FAILED)
			{
				printf("Service socket error while writing, closing thread.\n");
				goto socket_error;
			}

			ret_val = ReleaseSemaphore(
				leaderboard_semaphore,
				1, 		/* Signal that exactly one cell was emptied */
				NULL);
			if (ret_val == FALSE)
			{
				goto socket_error;
			}

			fclose(fp_leaderboard);
		}

		else if (STRINGS_ARE_EQUAL(AcceptedStr, "CLIENT_PLAYER_MOVE"))
		{
			if (server_play) //we play with server
			{
				winner = game(computer_move, user_move); //send to function to check who is the winner

				if (winner == 0) //It's a tie
					strcpy(winner_string, "Tie");

				else if (winner == 1)//computer won
				{
					if (write2leaderboard(username, 0) == SUCCESS_CODE) //update 1 lost for <username>
						strcpy(winner_string, "Server");
				}

				else if (winner == 2)//user won
				{
					if (write2leaderboard(username, 1) == SUCCESS_CODE) //update 1 won for <username>
						strcpy(winner_string, username);
				}

				sprintf(msg_to_send, "SERVER_GAME_RESULTS:%s;%s;%s;%s", "Server", computer_move, user_move, winner_string);

				SendRes = SendString(msg_to_send, *t_socket); //send to client who is the winner message
				if (SendRes == TRNS_FAILED)
				{
					printf("Service socket error while writing, closing thread.\n");
					goto socket_error;
				}

				SendRes = SendString("SERVER_GAME_OVER_MENU", *t_socket); //send to client game_over menu
				if (SendRes == TRNS_FAILED)
				{
					printf("Service socket error while writing, closing thread.\n");
					goto socket_error;
				}
			}

			else if (client_play)
			{
				/*****each user need to write his move into the file*******/
				//first to arrive, catch mutex write2file
				wait_code = WaitForSingleObject(write2file_mutex_handle, INFINITE);
				if (WAIT_OBJECT_0 != wait_code)
				{
					printf("Error when waiting for mutex\n");
					goto socket_error;
				}

				if (NULL == (fp_GameSession = fopen("GameSession.txt", "r")))//file is not exist, need to create! this is the first user to arrive here
				{
					if (NULL == (fp_GameSession = fopen("GameSession.txt", "w")))
					{
						printf("Error when open file");
						goto socket_error;
					}
					else //successful creating file, now write into it the move
					{
						fprintf(fp_GameSession, "%s%c", user_move, '\n');
						fclose(fp_GameSession);
					}

					// Release mutex so the other client will be able to write his move
					ret_val = ReleaseMutex(write2file_mutex_handle);
					if (FALSE == ret_val)
					{
						printf("Error when releasing\n");
						goto socket_error;
					}

					//wait here until the other user will write his move to file, using semaphore. At this point: gamesession_semaphore=0
					wait_res = WaitForSingleObject(gamesession_semaphore, INFINITE);
					if (wait_res != WAIT_OBJECT_0)
					{
						goto socket_error;
					}

					//open file again for reading
					if (NULL == (fp_GameSession = fopen("GameSession.txt", "a+"))) //open file for reading and appending
					{
						printf("Error when open file");
						goto socket_error;
					}

					else
					{
						fgets(other_move, sizeof(other_move), fp_GameSession);//my move from file
						fgets(other_move, sizeof(other_move), fp_GameSession);//opponent move from file
						fclose(fp_GameSession);
					}

					//delete GameSession.txt
					if (remove("GameSession.txt") != 0)
					{
						printf("An error has occured while deleting the file\n");
						goto socket_error;
					}
				}

				else //file is exists, the second clinet will arrive here
				{
					fclose(fp_GameSession);
					//open exists file in order to write the move
					if (NULL == (fp_GameSession = fopen("GameSession.txt", "a+"))) //open file for reading and appending
					{
						printf("Error when open file");
						goto socket_error;
					}
					else //filed opend ok
					{
						//read the other player move
						fgets(other_move, sizeof(other_move), fp_GameSession);//opponent move from file

						//write my move into file
						fprintf(fp_GameSession, "%s", user_move);
						fclose(fp_GameSession);
					}

					// Release mutex to the file
					ret_val = ReleaseMutex(write2file_mutex_handle);
					if (FALSE == ret_val)
					{
						printf("Error when releasing mutex\n");
						goto socket_error;
					}

					//release the first user from his semaphore
					ret_val = ReleaseSemaphore(
						gamesession_semaphore,
						1, 		/* Signal that exactly one cell was emptied */
						NULL);

					if (ret_val == FALSE)
					{
						printf("Error when releasing semaphore\n");
						goto socket_error;
					}
				}

				//remove '\n'
				for (int i = 0; i < strlen(other_move); i++)
				{
					if (other_move[i] == '\n')
						other_move[i] = '\0';
				}

				/****We have the move of each player****/
				winner = game(user_move, other_move);
				if (winner == 0) //tie
					strcpy(winner_string, "Tie"); 

				else if (winner == 1)//I won
				{
					if (write2leaderboard(username, 1) == SUCCESS_CODE) //update 1 won for <username>
						strcpy(winner_string, username);
				}

				else if (winner == 2)//opponent won
				{
					if (write2leaderboard(username, 0) == SUCCESS_CODE) //update 1 lost for <username>
						strcpy(winner_string, other_username);
				}

				sprintf(msg_to_send, "SERVER_GAME_RESULTS:%s;%s;%s;%s", other_username, other_move, user_move, winner_string);
				SendRes = SendString(msg_to_send, *t_socket); //send to client who is the winner message
				if (SendRes == TRNS_FAILED)
				{
					printf("Service socket error while writing, closing thread.\n");
					goto socket_error;
				}

				SendRes = SendString("SERVER_GAME_OVER_MENU", *t_socket); //send to client game_over menu
				if (SendRes == TRNS_FAILED)
				{
					printf("Service socket error while writing, closing thread.\n");
					goto socket_error;
				}
			}
		}

		else if (STRINGS_ARE_EQUAL(message_type, "CLIENT_PLAYER_MOVE"))
		{
			token = strtok(NULL, ":"); //extract user move
			strcpy(user_move, token);
		}

		else if (STRINGS_ARE_EQUAL(AcceptedStr, "CLIENT_REPLAY"))
		{
			//release the other client
			ret_val = ReleaseSemaphore(
				semaphores_arr[1 - my_index],
				1, 		/* Signal that exactly one cell was emptied */
				NULL);
			if (ret_val == FALSE)
			{
				goto socket_error;
			}

			//wait for the other client with semaphore
			wait_code = WaitForSingleObject(semaphores_arr[my_index], 15000);
			if (wait_code == WAIT_FAILED) //error
			{
				goto socket_error;
			}

			else if (wait_code == WAIT_TIMEOUT) //15 seconds passsed and the other client didn't replay, show main menu
			{
				sprintf(msg_to_send, "SERVER_OPPONEENT_QUIT:%S", other_username);
				SendRes = SendString(msg_to_send, *t_socket); //send to client who is the winner message
				if (SendRes == TRNS_FAILED)
				{
					printf("Service socket error while writing, closing thread.\n");
					goto socket_error;
				}
				sprintf(msg_to_send, "SERVER_MAIN_MENU");
			}
			else
			{
				sprintf(msg_to_send, "SERVER_PLAYER_MOVE_REQUEST");
			}

			SendRes = SendString(msg_to_send, *t_socket);

			if (SendRes == TRNS_FAILED)
			{
				printf("Service socket error while writing, closing thread.\n");
				goto socket_error;
			}
		}


		else if (STRINGS_ARE_EQUAL(AcceptedStr, "CLIENT_DISCONNECT")) //in the beginning and option number 4 in the main menu
		{
			Done = TRUE;
			//update users names if someone disconnect
			if (!strcmp(first_user, username))
				strcpy(first_user, "");
			else
				strcpy(second_user, "");

		}
	}
	//free(message_type);
	free(AcceptedStr);
	printf("Conversation ended.\n");
	closesocket(*t_socket);
	return SUCCESS_CODE;

socket_error:
	//free(message_type);
	free(AcceptedStr);
	closesocket(*t_socket);
	return ERROR_CODE;
}

/*
Description: The function checks if the user name is in the file already, if not: just add him to the file. If he is
		     there already we will create a file that will temp file in order to copy all the other users and update the
			 specific user. Then, the function sort the leaderboard file by ratio.
Parameters:  This function gets a username and a Bool that means if he won/lost. For won we will get win=1, for lost
			 we will get win=0.
Returns:	 0 if succeeded, -1 else.
*/

static int write2leaderboard(char* name, BOOL win)
{
	//variables
	DWORD wait_code=-1;
	BOOL user_is_here = FALSE, ret_val = FALSE, an_error_leader = FALSE;
	FILE* fp_temp = NULL;
	char* username = NULL, *username_temp = NULL;
	char line[MAX_LINE_IN_FILE_LEN] = "", line_temp[MAX_LINE_IN_FILE_LEN] = "", line_cpy[MAX_LINE_IN_FILE_LEN] = "", line2print[MAX_LINE_IN_FILE_LEN] = ""; 
	int curr_win = 0, curr_lost = 0, num_of_users = 0, user_num = 0;
	float max_ratio = 0, curr_ratio = 0;

	//first, catch mutex
	wait_code = WaitForSingleObject(write2file_mutex_handle, INFINITE);
	if (WAIT_OBJECT_0 != wait_code)
	{
		an_error_leader = TRUE;
		printf("Error when waiting for mutex\n");
		goto close_mutexs;
	}

	//now, catch semaphore
	wait_code = WaitForSingleObject(leaderboard_semaphore, INFINITE);
	if (wait_code != WAIT_OBJECT_0)
	{
		an_error_leader = TRUE;
		printf("Error when waiting for semaphore\n");
		goto close_semaphores;
	}

	/*We have mutex and semaphore*/

	//open file for appending and reading
	if (NULL == (fp_leaderboard = fopen("Leaderboard.csv", "a+")))
	{
		an_error_leader = TRUE;
		printf("Error when open file");
		goto close_semaphores;
	}

	//check if user is already in the file
	while (fgets(line, sizeof(line), fp_leaderboard) != NULL) //while loop over all the file
	{
		username = strtok(line, ","); //extract username
		if (!strcmp(username, name))
			user_is_here = TRUE;
	}

	if (!user_is_here) //we need to add a new user
	{
		if (win) //add 1 to Won
			sprintf(line2print, "%s,%d,%d, \n", name, 1, 0);
		else //add 1 to Lost
			sprintf(line2print, "%s,%d,%d, \n", name, 0, 1);

		fprintf(fp_leaderboard, line2print);
		fclose(fp_leaderboard); //DEBUG
	}

	else //user is already exists, just need to update
	{

		//open temp file again for appending
		if (NULL == (fp_temp = fopen("temp.csv", "a")))
		{
			//fclose(fp_leaderboard);
			an_error_leader = TRUE;
			printf("Error when open file");
			goto close_semaphores;
		}

		fprintf(fp_temp, "Name,Won,Lost,W/L Ratio\n"); //print first line

		rewind(fp_leaderboard); //update pointer to the beginning of file
		while (fgets(line, sizeof(line), fp_leaderboard) != NULL) //while loop over all the file
		{
			strcpy(line_cpy, line);
			username = strtok(line, ","); //extract username
			if (!strcmp(username, "Name"))
				continue;
			if (strcmp(username, name)) //this is not the name we need to update, so just copy to temp
				fprintf(fp_temp, line_cpy);
			else //this is the name we need to update
			{
				curr_win = atoi(strtok(NULL, ",")); //extract win stat
				curr_lost = atoi(strtok(NULL, ",")); //extract lost stat

				if (win) //add 1 to his win
					curr_win++;
				else //add 1 to his lost
					curr_lost++;

				//keep ratio empty if win/lost is 0
				if (curr_win == 0 || curr_lost == 0)
					sprintf(line2print, "%s,%d,%d, \n", name, curr_win, curr_lost);
				else
					sprintf(line2print, "%s,%d,%d,%.3f\n", name, curr_win, curr_lost, (float)curr_win / curr_lost);

				fprintf(fp_temp, line2print);
			}
		}

		fclose(fp_temp);
		fclose(fp_leaderboard);
		/*temp.csv is ready for copying into leaderboard.csv*/
		if (remove_leaderboard_and_rename_temp(user_is_here) == ERROR_CODE)
			goto close_semaphores;
	}

	user_is_here = FALSE;

	/*We have a file with the update, need to sort it by ratio*/
	//open file for appending and reading
	if (NULL == (fp_temp = fopen("temp.csv", "a+")))
	{
		an_error_leader = TRUE;
		printf("Error when open file");
		goto close_semaphores;
	}

	if (NULL == (fp_leaderboard = fopen("leaderboard.csv", "a+")))
	{
		an_error_leader = TRUE;
		printf("Error when open file");
		fclose(fp_temp);
		goto close_semaphores;
	}

	rewind(fp_leaderboard);
	while (fgets(line, sizeof(line), fp_leaderboard) != NULL) //while loop over all the file
		num_of_users++;
	num_of_users--;

	fprintf(fp_temp, "Name,Won,Lost,W/L Ratio\n");

	float* ratio_arr = NULL;
	float* sorted_ratio_arr = NULL;
	int i = 0;

	/* Memory Allocation */
	ratio_arr = (float*)malloc(num_of_users + 50); 
	if (NULL == ratio_arr)
	{
		an_error_leader = TRUE;
		printf("Memory allocation has failed!");
		goto free_malloc;
	}

	sorted_ratio_arr = (float*)malloc(num_of_users + 50);
	if (NULL == sorted_ratio_arr)
	{
		an_error_leader = TRUE;
		printf("Memory allocation has failed!");
		goto free_malloc;
	}

	//fill arrays with zeros
	for (int i = 0; i < sizeof(sorted_ratio_arr) - 1; i++)
	{
		ratio_arr[i] = 0;
		sorted_ratio_arr[i] = 0;
	}

	//fill ratio array
	rewind(fp_leaderboard);
	while (fgets(line, sizeof(line), fp_leaderboard) != NULL) //while loop over all the file
	{
		username = strtok(line, ","); //extract username
		curr_win = atoi(strtok(NULL, ",")); //extract win stat
		curr_lost = atoi(strtok(NULL, ",")); //extract lost stat
		curr_ratio = atof(strtok(NULL, ",")); //extract ratio stat

		if (!strcmp(username, "Name"))
			continue;
		ratio_arr[i] = curr_ratio;
		i++;
	}

	//sort ratio_array
	int helper = 0;
	for (int user_num = 0; user_num < num_of_users; user_num++)
	{
		for (int i = 0; i < sizeof(ratio_arr) - 1; i++)
		{
			if (ratio_arr[i] > max_ratio)
			{
				helper = i;
				max_ratio = ratio_arr[i];
			}
		}

		ratio_arr[helper] = -1; //reset the array index because we used the value already

		sorted_ratio_arr[user_num] = max_ratio; //the biggest ratio will be the first in the array
		max_ratio = 0; //reset max!!!!!
	}

	/*Write the sorted leaderboard by ratio to temp.csv*/
	for (int i = 0; i < sizeof(sorted_ratio_arr) - 1; i++)
	{
		rewind(fp_leaderboard); //update pointer to the beginning of file
		while (fgets(line, sizeof(line), fp_leaderboard) != NULL) //while loop over all the file
		{
			user_is_here = FALSE;
			strcpy(line_cpy, line);
			username = strtok(line, ","); //extract username
			curr_win = atoi(strtok(NULL, ",")); //extract win stat
			curr_lost = atoi(strtok(NULL, ",")); //extract lost stat
			curr_ratio = atof(strtok(NULL, ",")); //extract ratio stat

			if (!strcmp(username, "Name"))
				continue;
			if (curr_ratio == sorted_ratio_arr[i])
			{
				//check if user is already in the file
				rewind(fp_temp); //update pointer to the beginning of file
				while (fgets(line_temp, sizeof(line_temp), fp_temp) != NULL) //while loop over all the file
				{
					username_temp = strtok(line_temp, ","); //extract username
					if (!strcmp(username, username_temp))
						user_is_here = TRUE;
				}

				if (!user_is_here)
				{
					fprintf(fp_temp, line_cpy);
					break;
				}
			}
		}
	}

	fclose(fp_leaderboard);
	fclose(fp_temp);

	/*fp_temp is sorted*/
	if (remove_leaderboard_and_rename_temp(0) == ERROR_CODE)
		goto free_malloc;


	/*release mutex and semaphore, free malloc*/
free_malloc:
	free(ratio_arr);
	free(sorted_ratio_arr);
close_semaphores:
	ret_val = ReleaseSemaphore(
		leaderboard_semaphore,
		1, 		/* Signal that exactly one cell was emptied */
		NULL);
	if (ret_val == FALSE) {
		return ERROR_CODE;
	}

close_mutexs:
	ret_val = ReleaseMutex(write2file_mutex_handle);
	if (FALSE == ret_val)
	{
		an_error_leader = TRUE;
		printf("Error when releasing\n");
	}

	if (an_error_leader)
		return ERROR_CODE;
	else
		return SUCCESS_CODE;
}

/*
Description: This function update the temp file we created in order to sort, into leaderboard.
Parameters:  BOOL first enter or not.
Returns:	 0 if succeeded, -1 else.
*/

int remove_leaderboard_and_rename_temp(BOOL first)
{
	/*temp.csv is ready for copying into leaderboard.csv*/
	if (remove("Leaderboard.csv") != 0)	//delete Leaderboard.csv
	{
		an_error_occured_remove_LB = TRUE;
		printf("An error has occured while deleting the file\n");
		return ERROR_CODE;
	}

	if (rename("temp.csv", "Leaderboard.csv") && !first)//update new Leaderboard.csv
	{
		an_error_occured_remove_LB = TRUE;
		printf("Error when rename file name");
		return ERROR_CODE;
	}
	return SUCCESS_CODE;
}

/*
Description: The function that checks who is the winner in the game.
Parameters:  move: play1 and move: play2.
Returns:	 return 0 if tie, 1 if play1 won, 2 if play2 won.
*/

static int game(char* play1, char* play2) {

	int ret_val = -1;

	if (!strcmp(play1, play2))
		ret_val = 0;

	else if (!strcmp(play1, "ROCK"))
	{
		if (!strcmp(play2, "PAPER") || !strcmp(play2, "SPOCK"))
			ret_val = 2;
		else
			ret_val = 1;
	}

	else if (!strcmp(play1, "PAPER"))
	{
		if (!strcmp(play2, "SCISSORS") || !strcmp(play2, "LIZARD"))
			ret_val = 2;
		else
			ret_val = 1;
	}

	else if (!strcmp(play1, "SCISSORS"))
	{
		if (!strcmp(play2, "ROCK") || !strcmp(play2, "SPOCK"))
			ret_val = 2;
		else
			ret_val = 1;
	}

	else if (!strcmp(play1, "LIZARD"))
	{
		if (!strcmp(play2, "SCISSORS") || !strcmp(play2, "ROCK"))
			ret_val = 2;
		else
			ret_val = 1;
	}

	else if (!strcmp(play1, "SPOCK"))
	{
		if (!strcmp(play2, "LIZARD") || !strcmp(play2, "PAPER"))
			ret_val = 2;
		else
			ret_val = 1;
	}

	return ret_val;
}