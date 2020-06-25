/*
 This file was written with help of similar file from the course "Introduction to Systems Programming" at Tel-Aviv
 University, School of Electrical Engineering.
 */

#include "ClientIncludesAndDefines.h"
#include "SocketExampleShared.h"
#include "SocketSendRecvTools.h"

/*Global variables*/
SOCKET m_socket;
BOOL game_over_menu_server = FALSE, leaderboard_menu = FALSE;
char SendStr[MAX_MESSAGE_LEN]=""; //Can be in size of MAX_PLAY_MOVE_SIZE
HANDLE server_semaphore = NULL; //for semaphores
char server_ip[MAX_SERVER_IP]="";
char server_port[MAX_SERVER_PORT]="";

/*
Description: This is the function that will be recieving data coming from the server. Leaderboard implemented.
Parameters:  None.
Returns:	 exit program with 0 if everything went ok, with 0x555 else.
*/

static DWORD RecvDataThread(void)
{
	TransferResult_t RecvRes;
	BOOL an_error_occured = FALSE;

	while (1)
	{
		FILE* fp_helper = NULL;	//for leaderboard
		char* AcceptedStr = NULL;
		char* token = NULL;
		char* username = NULL;
		const char arguments[5][10]; //an array to store arguments from server message
		int win=-1, lost=-1, msg_to_send_capacity=MAX_MESSAGE_LEN;
		float ratio=-1;
		char line[MAX_CSV_LINE_LEN]="", line2print[MAX_CSV_LINE_LEN]="", MessageType[MAX_MESSAGE_LEN]="", *msg_to_send=NULL;

		RecvRes = ReceiveString(&AcceptedStr, m_socket);

		if (RecvRes == TRNS_FAILED)
		{
			printf("Socket error while trying to write data to socket\n");
			return SERVER_EXIT_ERROR;
		}

		else if (RecvRes == TRNS_DISCONNECTED)
		{
			printf("Server closed connection. Bye!\n");
			return 0x555;
		}

		else if (STRINGS_ARE_EQUAL(AcceptedStr, "SERVER_MAIN_MENU"))
			printf("Choose what to do next:\n1. Play against another client\n2. Play against the server\n3. View the leadeboard\n4. Quit\n");

		else if (STRINGS_ARE_EQUAL(AcceptedStr, "SERVER_PLAYER_MOVE_REQUEST"))
			printf("Choose a move from the list: Rock, Paper, Scissors, Lizard or Spock:\n");

		else if (STRINGS_ARE_EQUAL(AcceptedStr, "SERVER_GAME_OVER_MENU"))
		{
			game_over_menu_server = TRUE;
			printf("Choose what to do next:\n1. Play again\n2. Return to the main menu\n");
		}

		else if (STRINGS_ARE_EQUAL(AcceptedStr, "SERVER_NO_OPPONENTS"))
		{
			printf("There are no opponents right now\n");
		}

		else if (STRINGS_ARE_EQUAL(AcceptedStr, "SERVER_OPPONENT_QUIT"))
		{
			printf("%s has left the game!", arguments[0]);
		}

		//we have arguments
		else
		{
			token = strtok(AcceptedStr, ":"); //extract message_type
			strcpy(MessageType, token);

			if (STRINGS_ARE_EQUAL(MessageType, "SERVER_LEADERBOARD")) //we have just 1 long argument
			{
				leaderboard_menu = TRUE;
				token = strtok(NULL, ":"); //extract long argument

				//print token to help_file
				if (NULL == (fp_helper = fopen("help.csv", "w+")))
				{
					an_error_occured = TRUE;
					printf("Error when open file");
					goto clean_file;
				}

				fprintf(fp_helper, token);
				rewind(fp_helper);

				sprintf(line2print, "%s		%s		%s		%s\n", "Name", "Won", "Lost", "W/L Ratio");	//print first line
				printf(line2print);

				//extract line by line from file
				while (fgets(line, sizeof(line), fp_helper) != NULL) //while loop over all the file
				{
					username = strtok(line, ","); //extract username
					if (!strcmp(username, "Name"))// skip first line
						continue;
					win = atoi(strtok(NULL, ",")); //extract win stat
					lost = atoi(strtok(NULL, ",")); //extract lost stat
					ratio = atof(strtok(NULL, ",")); //extract ratio stat

					if (ratio != 0)
						sprintf(line2print, "%s		%d		%d		%.3f\n", username, win, lost, ratio);
					else
						sprintf(line2print, "%s		%d		%d		    \n", username, win, lost);
					printf(line2print);
				}

				fclose(fp_helper);
				if (remove("help.csv") != 0)//delete help.csv
				{
					an_error_occured = TRUE;
					printf("An error has occured while deleting the file\n");
				}

				//show client, leaderboard menu
				printf("Choose what to do next:\n1. Refresh leaderboard\n2. Return to main menu\n");
			}

			else
			{
				//extract all arguments
				int i = 0;
				while (token != NULL && i <= MAX_ARGUMENTS) 
				{
					token = strtok(NULL, ";");
					if (token == NULL)
						break;
					strcpy(arguments[i], token);
					i++;
				}
			}

			if (STRINGS_ARE_EQUAL(MessageType, "SERVER_GAME_RESULTS"))
			{
				msg_to_send_capacity = MAX_MESSAGE_LEN + strlen(arguments[0]) + strlen(arguments[1]) +strlen(arguments[2]) + strlen(arguments[3]);
			    msg_to_send = (char*)malloc(msg_to_send_capacity);
				if (NULL == msg_to_send) {
					printf("Memory allocation has failed!");
					free(AcceptedStr);
					return ERROR_CODE;
				}
				if (STRINGS_ARE_EQUAL(arguments[3], "Tie"))
					sprintf(msg_to_send, "You played: %s\n%s played: %s\n", arguments[2], arguments[0], arguments[1]);
				else
					sprintf(msg_to_send, "You played: %s\n%s played: %s\n%s won!\n", arguments[2], arguments[0], arguments[1], arguments[3]);

				printf("%s", msg_to_send);
				free(msg_to_send);
			}

			else if (STRINGS_ARE_EQUAL(MessageType, "SERVER_INVITE"))
				printf("A game is going to start against: %s\n", arguments[0]); 

			else if (STRINGS_ARE_EQUAL(MessageType, "SERVER_OPPONEENT_QUIT"))
				printf("%s has left the game!\n", arguments[0]); 
		}
		free(AcceptedStr);
	}
clean_file:
	if (an_error_occured)
		return ERROR_CODE; 
	return SUCCESS_CODE;
}

/*
Description: This is the function that will be sending data to the server. Leaderboard implemented.
Parameters:  None.
Returns:	 exit program with 0 if everything went ok, with 0x555 else.
*/

static DWORD SendDataThread(void)
{
	TransferResult_t SendRes;

	while (1)
	{
		gets_s(SendStr, sizeof(SendStr)+4); //reading a string from the keyboard

		int i = 0;
		char message_to_send[MAX_MESSAGE_LEN]=""; //const. size

		//change to uppercase if it's a move
		while (SendStr[i] != '\0')
		{
			if (SendStr[i] >= 'a' && SendStr[i] <= 'z')
				SendStr[i] = SendStr[i] - 32;
			i++;
		}

		/*****main menu*******/
		//user want to play against another user
		if (STRINGS_ARE_EQUAL(SendStr, "1") && !game_over_menu_server && !leaderboard_menu)
			sprintf(message_to_send, "CLIENT_VERSUS");

		else if (STRINGS_ARE_EQUAL(SendStr, "2") && !game_over_menu_server && !leaderboard_menu)
			sprintf(message_to_send, "CLIENT_CPU");

		else if (STRINGS_ARE_EQUAL(SendStr, "3"))
			sprintf(message_to_send, "CLIENT_LEADERBOARD");

		else if (STRINGS_ARE_EQUAL(SendStr, "4")) //decided to quit
		{
			sprintf(message_to_send, "CLIENT_DISCONNECT");
		}

		//send the user move
		else if (STRINGS_ARE_EQUAL(SendStr, "ROCK") || STRINGS_ARE_EQUAL(SendStr, "PAPER") || STRINGS_ARE_EQUAL(SendStr, "SCISSORS") || STRINGS_ARE_EQUAL(SendStr, "LIZARD") || STRINGS_ARE_EQUAL(SendStr, "SPOCK"))
			sprintf(message_to_send, "CLIENT_PLAYER_MOVE:%s", SendStr);

		/*****game_over menu_server*******/
		else if (STRINGS_ARE_EQUAL(SendStr, "1") && game_over_menu_server && !leaderboard_menu) //client wants to play another game
		{
			game_over_menu_server = FALSE; //need to update in order to be able to enter main menu options
			sprintf(message_to_send, "CLIENT_REPLAY");
		}
		else if (STRINGS_ARE_EQUAL(SendStr, "2") && game_over_menu_server && !leaderboard_menu) //client wants main menu
		{
			game_over_menu_server = FALSE; //need to update in order to be able to enter main menu options
			sprintf(message_to_send, "CLIENT_MAIN_MENU");
		}

		/*****leaderboard_menu*******/
		else if (STRINGS_ARE_EQUAL(SendStr, "1") && !game_over_menu_server && leaderboard_menu) //client wants to refresh leaderboard
		{
			leaderboard_menu = FALSE; //need to update in order to be able to enter main menu options
			sprintf(message_to_send, "CLIENT_REFRESH");
		}
		else if (STRINGS_ARE_EQUAL(SendStr, "2") && !game_over_menu_server && leaderboard_menu) //client wants main menu
		{
			leaderboard_menu = FALSE; //need to update in order to be able to enter main menu options
			sprintf(message_to_send, "CLIENT_MAIN_MENU");
		}

			SendRes = SendString(message_to_send, m_socket);
			if (SendRes == TRNS_FAILED)
			{

				printf("Socket error while trying to write data to socket\n");
				//return 0x555;
			}
	}
	return SUCCESS_CODE;
}

/*
Description: The fuction that is being called from the client_main. It connects the socket and generates Send and Recv.
			 Finally cleanups being made.
Parameters:  char *server_ip - the IP address.
			 char* server_port - the port (server).
			 char* username - the client's username. 
Returns:	 exit program with 0 if everything went ok, -1 (or through 0x555) else.
*/

int MainClient(char *server_ip_real, char* server_port_real, char* username)
{
	//int error_choice = -1;
	char error_choice[15] = "";
	DWORD wait_code;
	SOCKADDR_IN clientService;
	HANDLE hThread[2];
	char *SendStr=NULL;
	int SendStr_capacity = MAX_MESSAGE_LEN, exit_code_server = -1, exit_code_user_send = -1, exit_code_user_recv=-1;
	BOOL first_time = TRUE;
	BOOL ret_val = FALSE;
	TransferResult_t SendRes;
	TransferResult_t RecvRes;
	BOOL an_error_occured_loc = FALSE;
try_server:
	//copy arguments to global variables
	strcpy(server_ip, server_ip_real);
	strcpy(server_port, server_port_real);

	server_semaphore = CreateSemaphore(
		NULL,					/* Default security attributes */
		0,						/* Initial Count - all slots are empty */
		1,						/* Maximum Count */
		NULL);					/* named */

	if (server_semaphore == NULL)
	{
		an_error_occured_loc = TRUE;
		printf("An error has occured while creating the semaphore, Last Error = 0x%x\n!", GetLastError());
		goto close_semaphores;
	}

	// Initialize Winsock.
	WSADATA wsaData; //Create a WSADATA object called wsaData.
	//The WSADATA structure contains information about the Windows Sockets implementation.

	//Call WSAStartup and check for errors.
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR) {
		printf("Error at WSAStartup()\n");
		an_error_occured_loc = TRUE;
		goto close_semaphores;
	}

	//Call the socket function and return its value to the m_socket variable. 
	// For this application, use the Internet address family, streaming sockets, and the TCP/IP protocol.
try_to_reconnect:
	// Create a socket.
	m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check for errors to ensure that the socket is a valid socket.
	if (m_socket == INVALID_SOCKET)
	{
		printf("Error at socket(): %ld\n", WSAGetLastError());
		an_error_occured_loc = TRUE;
		goto close_semaphores;
	}
	/*
	 If the socket call fails, it returns INVALID_SOCKET.
	 The if statement in the previous code is used to catch any errors that may have occurred while creating
	 the socket. WSAGetLastError returns an error number associated with the last error that occurred.
	 */

	 //For a client to communicate on a network, it must connect to a server.
	 //Connect to a server.

	 //Create a sockaddr_in object clientService and set  values.
	clientService.sin_family = AF_INET;
	clientService.sin_addr.s_addr = inet_addr(server_ip); //Setting the IP address to connect to
	clientService.sin_port = htons(atoi(server_port)); //Setting the port to connect to.

	/* AF_INET is the Internet address family. */

	// Call the connect function, passing the created socket and the sockaddr_in structure as parameters. 
	// Check for general errors.
	//first connection with server

	while (1)
	{
		if (connect(m_socket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) //fail connect
		{
			if (first_time)
				printf("Failed connecting to server on %s:%s.\nChoose what to do next:\n1. Try to reconnect\n2. Exit\n", server_ip, server_port);
			else
			{
				printf("Connection to server on %s:%s has been lost.\nChoose what to do next:\n1. Try to reconnect \n2. Exit\nPlease choose twice :)\n", server_ip, server_port);
				first_time = TRUE;
			}

			//let's wait
			wait_code = WaitForSingleObject(server_semaphore, 500);

			gets_s(error_choice, 10);
			if (strcmp(error_choice,"1") == 0)
			{
				goto try_to_reconnect;
			}
			if (strcmp(error_choice, "2") == 0)
			{
				goto want_exit;
			}
		}

		else //successed connect
		{ 
			printf("Connected to server on %s:%s\n", server_ip, server_port);
			SendStr_capacity = MAX_MESSAGE_LEN + strlen(username);
			SendStr = (char*)malloc(SendStr_capacity);
			if (NULL == SendStr) {
				printf("Memory allocation has failed!");
				an_error_occured_loc = TRUE;
				goto want_exit;
			}
			sprintf(SendStr, "CLIENT_REQUEST:%s", username);
			SendRes = SendString(SendStr, m_socket);
			free(SendStr);
			if (SendRes == TRNS_FAILED)
			{
				printf("Socket error while trying to write data to socket\n");
				closesocket(m_socket);
				ret_val = CloseHandle(server_semaphore);
				if (FALSE == ret_val)
				{
					printf("Error when closing semaphore handle: %d\n", GetLastError());
					an_error_occured_loc = TRUE;
				}
				WSACleanup();
				return 0x555;
			}

			//wait for SERVER_APPROVED
			char* AcceptedStr = NULL;
			char* message_type = NULL;

			RecvRes = ReceiveString(&AcceptedStr, m_socket); 

			if (RecvRes == TRNS_FAILED)
			{
				printf("Socket error while trying to recieve data to socket\n");
				closesocket(m_socket);
				ret_val = CloseHandle(server_semaphore);
				if (FALSE == ret_val)
				{
					printf("Error when closing semaphore handle: %d\n", GetLastError());
					an_error_occured_loc = TRUE;
				}
				WSACleanup();
				return 0x555;
			}

			if (STRINGS_ARE_EQUAL(AcceptedStr, "SERVER_APPROVED"))
			{
				break;
			}

			if (STRINGS_ARE_EQUAL(AcceptedStr, "SERVER_DENIED"))
			{
				printf("Server on %s:%s denied the connection request.\n", server_ip, server_port);
			}
		}
	}

	// Send and receive data
	/*
		In this code, two integers are used to keep track of the number of bytes that are sent and received.
		The send and recv functions both return an integer value of the number of bytes sent or received,
		respectively, or an error. Each function also takes the same parameters:
		the active socket, a char buffer, the number of bytes to send or receive, and any flags to use.
	*/

	hThread[0] = CreateThread(
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)SendDataThread,
		NULL,
		0,
		NULL
	);

	if (NULL == hThread[0])
	{
		printf("Couldn't create Send thread\n");
		an_error_occured_loc = TRUE;
		goto want_exit;
	}

	hThread[1] = CreateThread(
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)RecvDataThread,
		NULL,
		0,
		NULL
	);

	if (NULL == hThread[1])
	{
		printf("Couldn't create Recv thread\n");
		an_error_occured_loc = TRUE;
		goto want_exit;
	}

	WaitForMultipleObjects(2, hThread, FALSE, INFINITE);

	wait_code = WaitForSingleObject(server_semaphore, 500);

	/*GetExitCodeThread(hThread[0], &exit_code_user_send);
	if (exit_code_user_send == 0x555)
		an_error_occured_loc = TRUE;*/

	GetExitCodeThread(hThread[1], &exit_code_user_recv);
	GetExitCodeThread(hThread[1], &exit_code_server);

	if (exit_code_user_recv == ERROR_CODE)
		an_error_occured_loc = TRUE;
	
	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);

want_exit:
	closesocket(m_socket);

close_semaphores:
	ret_val = CloseHandle(server_semaphore);
	if (FALSE == ret_val)
	{
		printf("Error when closing semaphore handle: %d\n", GetLastError());
		an_error_occured_loc = TRUE;
	}

	WSACleanup();

	if (exit_code_server == SERVER_EXIT_ERROR)
	{
		first_time = FALSE;
		goto try_server;
	}

	if (an_error_occured_loc)
		return ERROR_CODE;

	return SUCCESS_CODE;
}
