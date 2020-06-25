/*
Authors: Tomer Marguy...........id_number: 205705874
		 Gavriel Rashkovsky.....id_number: 312178999
Project name: "group49_ex4_server"
*/

#include "ServerIncludesAndDefines.h"

/*
Description: This is the main function of group49_ex4_server. It will execute when group49_ex4_server will create
			 a new process.
			 argv[1] will be the <server_port>, the port.
Parameters:  int argc - the number of strings pointed to by argv.
			 char *argv[] - an array of strings representing the individual arguments provided on the command line.
Returns:	 exit program with 0 if everything went ok, -1 else.
*/

int main(int argc, char* argv[])
{
	//variable initiation
	char server_port[MAX_SERVER_PORT]="";

	//extract server port
	strcpy(server_port, argv[1]);

	return MainServer(server_port);
}
