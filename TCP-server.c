#ifdef _WIN32
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <winsock2.h> //for all socket programming
#include <ws2tcpip.h> //for getaddrinfo, inet_pton, inet_ntop
#include <stdio.h> //for fprintf, perror
#include <unistd.h> //for close
#include <stdlib.h> //for exit
#include <string.h> //for memset
#include <time.h>
#include <stdint.h>

void OSInit(void) {
    WSADATA wsaData;
    int WSAError = WSAStartup(MAKEWORD(2, 0), &wsaData); 
    if (WSAError!= 0) {
        fprintf(stderr, "WSAStartup errno = %d\n", WSAError);
        exit(-1);
    }
}

void OSCleanup(void) {
    WSACleanup();
}

#define perror(string) fprintf(stderr, string ": WSA errno = %d\n", WSAGetLastError())
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
#include <time.h>
#include <stdint.h>

void OSInit(void) {}
void OSCleanup(void) {}
#endif

int initialization();
int connection(int internet_socket, fd_set *master, int *fdmax);
void execution(int client_socket, fd_set *master, int *fdmax, uint32_t *random_number);
void cleanup(int internet_socket, fd_set *master);

int main(int argc, char *argv[]) {
    OSInit();
    printf("start TCP server\n");

    int internet_socket = initialization();
    if (internet_socket == -1) {
        fprintf(stderr, "Failed to initialize. Exiting...\n");
        return 1;
    }

    fd_set master;
    fd_set read_fds;
    int fdmax;

    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(internet_socket, &master);

    fdmax = internet_socket;

    //Generate a random number
    srand(time(NULL));
    uint32_t random_number = rand() % 1000000 + 1;
    printf("Random number generated: %u\n", random_number);

    //Main loop
    while (1) {
        read_fds = master;
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            cleanup(internet_socket, &master);
            return 2;
        }
        //Run through the existing connections looking for data to read
        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == internet_socket) {
                    connection(internet_socket, &master, &fdmax);
                } else {
                    execution(i, &master, &fdmax, &random_number);
                }
            }
        }
    }

    //Clean up 
    cleanup(internet_socket, &master);
    printf("End TCP server!");

    return 0;
}

int initialization() {
    //Get address information
    struct addrinfo internet_address_setup;
    struct addrinfo *internet_address_result;
    memset(&internet_address_setup, 0, sizeof(internet_address_setup));
    internet_address_setup.ai_family = AF_UNSPEC;
    internet_address_setup.ai_socktype = SOCK_STREAM;
    internet_address_setup.ai_flags = AI_PASSIVE;
    int getaddrinfo_return = getaddrinfo(NULL, "24042", &internet_address_setup, &internet_address_result);
    if (getaddrinfo_return!= 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(getaddrinfo_return));
        return -1;
    }

    int internet_socket = -1;
    struct addrinfo *internet_address_result_iterator = internet_address_result;
    while (internet_address_result_iterator!= NULL) {
        //Create socket
        internet_socket = socket(internet_address_result_iterator->ai_family, internet_address_result_iterator->ai_socktype, internet_address_result_iterator->ai_protocol);
        if (internet_socket == -1) {
            perror("socket");
        } else {
            //Bind socket to an address
            if (bind(internet_socket, internet_address_result_iterator->ai_addr, internet_address_result_iterator->ai_addrlen) == -1) {
                perror("bind");
close(internet_socket);
            } else {
                //Listen for incoming connections
                if (listen(internet_socket, SOMAXCONN) == -1) {
                    close(internet_socket);
                    perror("listen");
                } else {
                    break;
                }
            }
        }
        internet_address_result_iterator = internet_address_result_iterator->ai_next;
    }

    freeaddrinfo(internet_address_result);

    if (internet_socket == -1) {
        fprintf(stderr, "socket: no valid socket address found\n");
        return -1;
    }

    return internet_socket;
}

int connection(int internet_socket, fd_set *master, int *fdmax) {
    //Handle new connections
    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(client_addr);
    int client_socket = accept(internet_socket, (struct sockaddr *)&client_addr, &addr_size);
    if (client_socket == -1) {
        perror("accept");
    } else {
        char client_ip[INET6_ADDRSTRLEN];
        inet_ntop(client_addr.ss_family, &((struct sockaddr_in *)&client_addr)->sin_addr, client_ip, sizeof client_ip);

        FD_SET(client_socket, master);
        if (client_socket > *fdmax) {
            *fdmax = client_socket;
        }
        printf("new connection from %s on socket %d\n", client_ip, client_socket);
        send(client_socket, "Guess a number between 1 and 1000000.", 64, 0);
    }

    return client_socket;
}

void execution(int client_socket, fd_set *master, int *fdmax, uint32_t *random_number) {
    ssize_t bytes_read;
    char client_guess_str[32];

    bytes_read = recv(client_socket, client_guess_str, sizeof(client_guess_str) - 1, 0);
    if (bytes_read <= 0) {
        if (bytes_read == -1) {
            //Client disconnected
            char client_ip[INET6_ADDRSTRLEN];
            struct sockaddr_storage client_addr;
            socklen_t addr_size = sizeof(client_addr);
            getpeername(client_socket, (struct sockaddr *)&client_addr, &addr_size);
            inet_ntop(client_addr.ss_family, &((struct sockaddr_in *)&client_addr)->sin_addr, client_ip, sizeof client_ip);
            printf("Client %s on socket %d hung up\n", client_ip, client_socket);
        } else {
            perror("recv");
        }
        close(client_socket);
        FD_CLR(client_socket, master);
    } else {
        client_guess_str[bytes_read] = '\0';
        uint32_t client_guess_network = ntohl(*(uint32_t *)client_guess_str); //Convert from network byte order to host byte order
        if (client_guess_network < 1 || client_guess_network > 1000000) {
            send(client_socket, "Number out of range", 20, 0);
            return;
        }

        printf("Received from client: %u\n", client_guess_network);

        if (client_guess_network > *random_number) {
            send(client_socket, "Lower", 6, 0);
        } else if (client_guess_network < *random_number) {
            send(client_socket, "Higher", 7, 0);
        } else {
            send(client_socket, "Correct", 8, 0);
            printf("Client won! Closing connection.\n");

            send(client_socket, "Game Over: You guessed the correct number!", 42, 0);close(client_socket);
            FD_CLR(client_socket, master);

            //Generate a new random number
            *random_number = rand() % 1000000 + 1;
            printf("New random number generated: %u\n", *random_number);

            //Update the random number for the remaining clients
            for (int i = 0; i <= *fdmax; i++) {
                if (FD_ISSET(i, master) && i!= client_socket) {
                    send(i, "New game started! Guess a number between 1 and 1000000.", 64, 0);
                }
            }
        }
    }
}

void cleanup(int internet_socket, fd_set *master) {
    close(internet_socket);
    for (int i = 0; i <= FD_SETSIZE; ++i) {
        if (FD_ISSET(i, master)) {
            close(i);
            FD_CLR(i, master);
        }
    }
    OSCleanup();
}
