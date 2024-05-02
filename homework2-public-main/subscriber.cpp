#include <iostream>
#include <vector>
#include <bits/stdc++.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "helper.h"

using namespace std;

/**
 * Function splitting given message into an array of strings
 * */
int split_message(char *message, vector<string> &tokens) {
    char buffer[BUF_LEN + 1];
    memcpy(buffer, message, BUF_LEN);
    buffer[BUF_LEN] = '\n';
    string str = buffer;
    string token;
    std::stringstream str_stream(str);

    for (int i = 0; i < BUF_LEN; i++) {
        if (buffer[i] == '\n') {
            getline(str_stream, token, '\n');
            tokens.push_back(token);
        }

        // no partial message
        if (buffer[i + 1] == '\0') {
            return 1;
        }
    }

    std::getline(str_stream, token, '\n');
    tokens.push_back(token);

    return 0;
}

///**
// * Function sending client's ID to the server
// * **/
//void send_id_to_server(int sockfd, char *id) {
//    int n = send(sockfd, id, strlen(id), 0);
//    DIE(send(sockfd, id, strlen(id), 0) < 0, "send");
//}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    DIE(argc != 4, "Invalid number of arguments!\nUsage: ./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER>");

    struct sockaddr_in serv_addr;
    char buffer[BUF_LEN];

    fd_set read_fds, tmp_fds;

    // empty set of read descriptors (read_fds) and the temporary set (tmp_fds)
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "Error when creating TCP socket.");

    // disable Nagle algorithm
    int nagle = 1;
    DIE(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nagle, sizeof(nagle)) < 0,
        "Error when disabling the Nagle algorithm.");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));

    DIE(inet_aton(argv[2], &serv_addr.sin_addr) == 0, "Error when calling inet_aton for finding the server address.");

    // create connection to server
    DIE(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0,
        "Error when connecting socket to server.");

    DIE(send(sockfd, argv[1], strlen(argv[1]), 0) < 0, "Error when sending the client ID to the server.");

    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(sockfd, &read_fds);

    while (true) {
        tmp_fds = read_fds;
        DIE(select(sockfd + 1, &tmp_fds, NULL, NULL, NULL) < 0, "Error when calling select.");

        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
            // read data from STDIN
            memset(buffer, 0, BUF_LEN);
            fgets(buffer, BUF_LEN - 1, stdin);

            // send the message to server
            int n = send(sockfd, buffer, strlen(buffer), 0);
            DIE(n < 0, "Error when sending message to the server.");

            // perform exit/forceful shut command
            if (n == 0 || strncmp(buffer, "exit", 4) == 0) {
                break;
            }
        }

        if (FD_ISSET(sockfd, &tmp_fds)) {
            // data was received from server
            string tmp = "";
            memset(buffer, 0, BUF_LEN);

            int nr_bytes_read = recv(sockfd, buffer, BUF_LEN, 0);
            DIE(nr_bytes_read < 0, "Error when receiving message from the server.");

            if (nr_bytes_read == 0) {
                // server forcefully shut
                // client connection will also be closed
                break;
            }

            if (strncmp(buffer, "exit", 4) == 0) {
                // end connection for exit command
                break;
            }

            while (nr_bytes_read != 0) {
                char buff[BUF_LEN];

                if (buffer[0] == '\n') {
                    // end of buffer reached
                    break;
                }

                // split message based on new lines
                vector<string> strings;
                int ret = split_message(buffer, strings);

                if (tmp.empty()) {
                    // no partial message

                    // write all messages to STDOUT besides the last one
                    // which could be partial
                    for (int i = 0; i < strings.size() - 1; i++) {
                        cout << strings[i] << "\n";
                    }
                } else {
                    // pending partial message
                    tmp += strings[0];
                    // write the now completed message
                    cout << tmp << '\n';

                    // write all other messages to STDOUT besides the last one
                    // which could be partial
                    for (int i = 1; i < strings.size() - 1; i++) {
                        cout << strings[i] << "\n";
                    }

                    // no partial message
                    tmp.clear();

                    if (ret == 1 && strings.size() == 1) {
                        break;
                    }
                }

                if (ret == 0) {
                    // partial message found
                    tmp += strings[strings.size() - 1];
                } else {
                    // no partial message, which means end of buffer
                    cout << strings[strings.size() - 1] << "\n";
                    break;
                }

                // receive data from server
                memset(buffer, 0, BUF_LEN);
                nr_bytes_read = recv(sockfd, buffer, BUF_LEN, 0);
                DIE(nr_bytes_read < 0, "Error when receiving message from the server.");
            }

        }
    }

    // Close the TCP socket.
    close(sockfd);

    return 0;
}
