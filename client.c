/*
    Héctor Mauricio González Coello
    A01328258
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// Sockets libraries
#include <netdb.h>
#include <arpa/inet.h>
// Poll library
#include <sys/poll.h>
// Custom libraries
#include "sockets.h"
#include "fatal_error.h"

#define BUFFER_SIZE 1024

///// FUNCTION DECLARATIONS
void usage(char * program);

///// MAIN FUNCTION
int main(int argc, char * argv[])
{
    int connection_fd;

    // Check the correct arguments
    if (argc != 3)
    {
        usage(argv[0]);
    }

    // Start the server
    connection_fd = connectSocket(argv[1], argv[2]);
	// Use the service available operation s

    // Close the socket
    close(connection_fd);

    return 0;
}

///// FUNCTION DEFINITIONS

/*
    Explanation to the user of the parameters required to run the program
*/
void usage(char * program)
{
    printf("Usage:\n");
    printf("\t%s {server_address} {port_number}\n", program);
    exit(EXIT_FAILURE);
}
