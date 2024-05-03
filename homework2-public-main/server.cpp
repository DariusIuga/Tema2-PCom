#include <iostream>
#include <vector>
#include <map>
#include <bits/stdc++.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "server_helper.h"

using namespace std;

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    DIE(argc < 2, "arguments");

    int socket_TCP, socket_UDP, newsockfd, portno, dest;
    char buffer[BUF_LEN];
    char client_id_buff[BUF_LEN];
    struct sockaddr_in serv_addr, cli_addr, udp_addr;
    int n, i, ret;
    socklen_t clilen, udplen;

    unordered_map<string, client *> map_id_clients;
    unordered_map<int, client *> map_connected_clients;
    vector<topic> topics;

    fd_set read_fds;    // set for reading used in select()
    fd_set tmp_fds;        // set used temporarily
    int fdmax;            // maximum value of fd from read_fds set

    // empty set of read descriptors (read_fds) and the temporary set (tmp_fds)
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    // create TCP socket
    socket_TCP = socket(AF_INET, SOCK_STREAM, 0);
    DIE(socket_TCP < 0, "TCP socket");

    // disable Nagle algorithm
    int nagle = 1;
    ret = setsockopt(socket_TCP, IPPROTO_TCP, TCP_NODELAY, &nagle, sizeof(nagle));
    DIE(ret < 0, "Nagle");

    portno = atoi(argv[1]);
    DIE(portno == 0, "atoi");

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // bind TCP client socket to port
    ret = bind(socket_TCP, (struct sockaddr *) &serv_addr,
               sizeof(struct sockaddr));
    DIE(ret < 0, "TCP bind");

    // enable reuse port action
    int reuse_port = 1;
    ret = setsockopt(socket_TCP, SOL_SOCKET, SO_REUSEADDR, &reuse_port, sizeof(reuse_port));
    DIE(ret < 0, "Reuse port");

    // listen on the TCP client socket
    ret = listen(socket_TCP, MAX_CLIENTS);
    DIE(ret < 0, "TCP listen");

    // create UDP socket
    socket_UDP = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(socket_UDP < 0, "UDP socket");

    // bind UDP client socket to port
    ret = bind(socket_UDP, (struct sockaddr *) &serv_addr,
               sizeof(struct sockaddr));
    DIE(ret < 0, "UDP bind");

    // the sockets for TCP clients, UDP clients and STDIN are added to the read_fds set
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(socket_TCP, &read_fds);
    FD_SET(socket_UDP, &read_fds);
    fdmax = max(socket_TCP, socket_UDP);

    while (1) {
        tmp_fds = read_fds;

        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "select");

        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
            // read data from STDIN
            memset(buffer, 0, BUF_LEN);
            fgets(buffer, BUF_LEN - 1, stdin);
            buffer[strlen(buffer) - 1] = '\0';

            // check if server received exit command
            if (buffer == NULL || strlen(buffer) == 0 || strncmp(buffer, "exit", 4) == 0) {
                // close all clients
                close_clients(map_connected_clients, map_id_clients, buffer);
                break;
            }

            continue;
        }

        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds)) {
                if (i != socket_UDP) {
                    // client TCP actions
                    if (i == socket_TCP) {
                        // accept a connection request from the inactive socket
                        clilen = sizeof(cli_addr);
                        newsockfd = accept(socket_TCP, (struct sockaddr *) &cli_addr, &clilen);
                        DIE(newsockfd < 0, "accept");

                        // socket is added to the set of read descriptors
                        FD_SET(newsockfd, &read_fds);
                        if (newsockfd > fdmax) {
                            fdmax = newsockfd;
                        }

                        // receive ID from client
                        memset(buffer, 0, BUF_LEN);
                        int n = recv(newsockfd, buffer, sizeof(buffer), 0);
                        DIE(n < 0, "recv");

                        // create connection
                        connect_client(buffer, inet_ntoa(cli_addr.sin_addr),
                                       ntohs(cli_addr.sin_port),
                                       map_connected_clients, map_id_clients,
                                       newsockfd, read_fds);

                        fflush(stdout);
                    } else {
                        // receive message from subscriber
                        memset(buffer, 0, BUF_LEN);
                        int n = recv(i, buffer, sizeof(buffer), 0);
                        DIE(n < 0, "recv");
                        buffer[strlen(buffer) - 1] = '\0';

                        if (n == 0 || strncmp(buffer, "exit", 4) == 0) {
                            // client received exit/forceful shut command
                            // client is disconnected from server
                            disconnect_client(i, map_connected_clients, map_id_clients);
                            close(i);
                            FD_CLR(i, &read_fds);
                        } else {
                            // process message and execute command
                            execute_tcp_client_command(i, buffer, topics,
                                                       map_connected_clients, map_id_clients);
                        }

                    }
                } else {
                    // UDP client actions
                    memset(buffer, 0, BUF_LEN);
                    udplen = sizeof(udp_addr);
                    // receive message from UDP client
                    ret = recvfrom(socket_UDP, buffer, BUF_LEN, 0,
                                   (struct sockaddr *) &udp_addr, &udplen);
                    DIE(ret < 0, "UDP receive");
                    // UDP client was closed so the program moves on
                    if (ret == 0) {
                        continue;
                    }

                    std::string ip(inet_ntoa(udp_addr.sin_addr));
                    int port = ntohs(udp_addr.sin_port);

                    // create an UDP packet
                    packet_UDP packet = create_udp_package(buffer, port, ip);

                    // the packet's formatted message is not valid
                    if (packet.formatted_message.empty()) {
                        continue;
                    }

                    // send the UDP message to respective clients
                    send_udp_message(packet, topics);
                }
            }
        }
    }

    close(socket_TCP);

    return 0;
}
