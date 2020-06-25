/*
 This file was written with help of similar file from the course "Introduction to Systems Programming" at Tel-Aviv
 University, School of Electrical Engineering.
*/

#ifndef SOCKET_EXAMPLE_SHARED_H
#define SOCKET_EXAMPLE_SHARED_H

#define SERVER_ADDRESS_STR "127.0.0.1"
#define SERVER_PORT 2345
#define MAX_PLAY_MOVE_SIZE 10
#define MAX_MESSAGE_LEN 256
#define MAX_USERNAME_LEN 21

#define STRINGS_ARE_EQUAL( Str1, Str2 ) ( strcmp( (Str1), (Str2) ) == 0 )

#endif // SOCKET_EXAMPLE_SHARED_H
