#ifdef _WIN32
	#define _WIN32_WINNT _WIN32_WINNT_WIN7
	#include <winsock2.h> //for all socket programming
	#include <ws2tcpip.h> //for getaddrinfo, inet_pton, inet_ntop
	#include <stdio.h> //for fprintf, perror
	#include <unistd.h> //for close
	#include <stdlib.h> //for exit
	#include <string.h> //for memset
	#include <time.h> //for timer
	void OSInit( void )
	{
		WSADATA wsaData;
		int WSAError = WSAStartup( MAKEWORD( 2, 0 ), &wsaData ); 
		if( WSAError != 0 )
		{
			fprintf( stderr, "WSAStartup errno = %d\n", WSAError );
			exit( -1 );
		}
	}
	void OSCleanup( void )
	{
		WSACleanup();
	}
	#define perror(string) fprintf( stderr, string ": WSA errno = %d\n", WSAGetLastError() )
#else
	#include <sys/socket.h> //for sockaddr, socket, socket
	#include <sys/types.h> //for size_t
	#include <netdb.h> //for getaddrinfo
	#include <netinet/in.h> //for sockaddr_in
	#include <arpa/inet.h> //for htons, htonl, inet_pton, inet_ntop
	#include <errno.h> //for errno
	#include <stdio.h> //for fprintf, perror
	#include <unistd.h> //for close
	#include <stdlib.h> //for exit
	#include <string.h> //for memset
	#include <time.h> //for timer
	void OSInit( void ) {}
	void OSCleanup( void ) {}
#endif

int initialization( struct sockaddr ** internet_address, socklen_t * internet_address_length );
void execution( int internet_socket, struct sockaddr * internet_address, socklen_t internet_address_length );
void cleanup( int internet_socket, struct sockaddr * internet_address );

int main( int argc, char * argv[] )
{
	//////////////////
	//Initialization//
	//////////////////

	OSInit();

	struct sockaddr * internet_address = NULL;
	socklen_t internet_address_length = 0;
	int internet_socket = initialization( &internet_address, &internet_address_length );

	/////////////
	//Execution//
	/////////////

	execution( internet_socket, internet_address, internet_address_length );


	////////////
	//Clean up//
	////////////

	cleanup( internet_socket, internet_address );

	OSCleanup();

	return 0;
}

int initialization( struct sockaddr ** internet_address, socklen_t * internet_address_length )
{
	//Adresinformatie ophalen
	struct addrinfo internet_address_setup;
	struct addrinfo * internet_address_result;
	memset( &internet_address_setup, 0, sizeof internet_address_setup );
	internet_address_setup.ai_family = AF_UNSPEC;
	internet_address_setup.ai_socktype = SOCK_DGRAM;
	int getaddrinfo_return = getaddrinfo( "::1", "24042", &internet_address_setup, &internet_address_result );
	if( getaddrinfo_return != 0 )
	{
		fprintf( stderr, "getaddrinfo: %s\n", gai_strerror( getaddrinfo_return ) );
		exit( 1 );
	}

	int internet_socket = -1;
	struct addrinfo * internet_address_result_iterator = internet_address_result;
	while( internet_address_result_iterator != NULL )
	{
		//Socket maken
		internet_socket = socket( internet_address_result_iterator->ai_family, internet_address_result_iterator->ai_socktype, internet_address_result_iterator->ai_protocol );
		if( internet_socket == -1 )
		{
			perror( "socket" );
		}
		else
		{
			//Internetadres en -lengte instellen
			*internet_address_length = internet_address_result_iterator->ai_addrlen;
			*internet_address = (struct sockaddr *) malloc( internet_address_result_iterator->ai_addrlen );
			memcpy( *internet_address, internet_address_result_iterator->ai_addr, internet_address_result_iterator->ai_addrlen );
			break;
		}
		internet_address_result_iterator = internet_address_result_iterator->ai_next;
	}

	freeaddrinfo( internet_address_result );

	if( internet_socket == -1 )
	{
		fprintf( stderr, "socket: no valid socket address found\n" );
		exit( 2 );
	}

	return internet_socket;
}

void execution(int internet_socket, struct sockaddr *internet_address, socklen_t internet_address_length) {
    int timeout_seconds = 2;
	int number_of_bytes_received = 0;
	char buffer[1000];

    while (1) {
        // Read input from user
        printf("Enter a number to send (or enter 100 to quit): ");
        int number_to_send;
        scanf("%d", &number_to_send);

        if (number_to_send == 100) // Exit if 100 is entered
            break;

        // Convert integer to ASCII
        char number_str[20];
        if (number_to_send >= 0 && number_to_send <= 99) {
            if (number_to_send < 10) {
                sprintf(number_str, "%d", number_to_send); 
            } else {
                sprintf(number_str, "%02d", number_to_send);
            }
        } else {
            printf("Invalid number. Please enter a number between 0 and 99.\n");
            continue;
        }

        // Send to server
        int number_of_bytes_send = sendto(internet_socket, number_str, strlen(number_str), 0, internet_address, internet_address_length);
        if (number_of_bytes_send == -1) {
            perror("sendto");
            break;
        }

        // Wait for response from server with timeout
        char buffer[1000];
        int number_of_bytes_received = 0;
        struct timeval timeout;
        timeout.tv_sec = timeout_seconds;
        timeout.tv_usec = 0;
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(internet_socket, &read_fds);
        int select_result = select(internet_socket + 1, &read_fds, NULL, NULL, &timeout);
        if (select_result == -1) {
            perror("select");
            break;
        } else if (select_result == 0) {
            printf("Timeout occurred. No response from server.\n");
            continue;
        }

        // Data is available
        number_of_bytes_received = recvfrom(internet_socket, buffer, sizeof(buffer) - 1, 0, internet_address, &internet_address_length);
        if (number_of_bytes_received == -1) {
            perror("recvfrom");
            break;
        } else if (number_of_bytes_received > 0) {
            // Data successfully received
            buffer[number_of_bytes_received] = '\0';
            if (strcmp(buffer, "You Won ?") == 0) {
                printf("Congratulations, you won!\n");
            } else if (strcmp(buffer, "You lost ?") == 0) {
                printf("You lost.\n");
            } else {
                printf("Received: %s\n", buffer);
            }
        }
    }
    // Receive any remaining messages from the server
    number_of_bytes_received = recvfrom(internet_socket, buffer, sizeof(buffer) - 1, 0, internet_address, &internet_address_length);
    if (number_of_bytes_received == -1) {
        perror("recvfrom");
    } else {
        buffer[number_of_bytes_received] = '\0';
        if (strcmp(buffer, "You Won ?") == 0) {
            printf("Congratulations, you won!\n");
        } else if (strcmp(buffer, "You lost ?") == 0) {
            printf("You lost.\n");
        } else {
            printf("Received: %s\n", buffer);
        }
    }
}

void cleanup( int internet_socket, struct sockaddr * internet_address )
{
	free( internet_address );
	close( internet_socket );
}
