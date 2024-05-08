#include <iostream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "helper.hpp"

using namespace std;

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    DIE(argc != 4, "Invalid number of arguments!\nUsage: ./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER>");

    sockaddr_in server_address;
    char buffer[BUF_LEN];

    fd_set read_fds, tmp_fds;

    // Initialize the sets
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "Error when creating TCP socket.");

    // Disable the Nagle algorithm
    int nagle = 1;
    DIE(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nagle, sizeof(nagle)) < 0,
        "Error when disabling the Nagle algorithm.");

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(atoi(argv[3]));

    DIE(inet_aton(argv[2], &server_address.sin_addr) == 0,
        "Error when calling inet_aton for finding the server address.");

    // Create connection to server
    DIE(connect(sockfd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0,
        "Error when connecting socket to server.");

    DIE(send(sockfd, argv[1], strlen(argv[1]), 0) < 0, "Error when sending the client ID to the server.");

    // Add stdin and the tcp socket to the read set
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(sockfd, &read_fds);

    while (true) {
        tmp_fds = read_fds;
        DIE(select(sockfd + 1, &tmp_fds, NULL, NULL, NULL) < 0, "Error when calling select on one of the TCP clients.");
        // Clear previous buffer
        memset(buffer, 0, BUF_LEN);

        // We have some data to read from the TCP socket
        if (FD_ISSET(sockfd, &tmp_fds)) {
            // Fetch data from the server
            int nr_bytes_read = recv(sockfd, buffer, BUF_LEN, 0);
            DIE(nr_bytes_read < 0, "Error when receiving message from the server.");
            printf("%s", buffer);

            if (nr_bytes_read == 0 || strncmp(buffer, "exit", 4) == 0) {
                // Server forcefully shut or the server acknowledged the exit command
                // Close the client connection
                break;
            }
        }

        // We have some data to read from stdin
        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
            fgets(buffer, BUF_LEN - 1, stdin);

            // send the message to server
            int command_len = send(sockfd, buffer, strlen(buffer), 0);
            DIE(command_len < 0, "Error when sending message to the server.");

            // perform exit/forceful shut command
            if (command_len == 0 || strncmp(buffer, "exit", 4) == 0) {
                break;
            }
        }
    }

    // Close the TCP socket.
    close(sockfd);

    return 0;
}
