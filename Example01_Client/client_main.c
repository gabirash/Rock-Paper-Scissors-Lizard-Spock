/*
Authors: Tomer Marguy...........id_number: 205705874
		 Gavriel Rashkovsky.....id_number: 312178999
Project name: "group49_ex4_client"
*/

#include "ClientIncludesAndDefines.h"

/*
Description: This is the main function of group49_ex4_client. It will execute when group49_ex4_client will create
			 a new process.
			 argv[1] will be the <server_ip>, the IP address.  argv[2] will be the <server_port>, the port.
			 argv[3] will be the <username>, the client's username.
Parameters:  int argc - the number of strings pointed to by argv.
			 char *argv[] - an array of strings representing the individual arguments provided on the command line.
Returns:	 exit program with 0 if everything went ok, -1 or 0x555 else.
*/

int main(int argc, char* argv[])
{
	//variables initiations
	char server_ip[MAX_SERVER_IP]="", server_port[MAX_SERVER_PORT]="", username[MAX_USERNAME_LEN]="";
	int ret_val = -50;

	//extract server ip, server port and username
	strcpy(server_ip, argv[1]);
	strcpy(server_port, argv[2]);
	strcpy(username, argv[3]);

	return MainClient(server_ip, server_port, username);
}
