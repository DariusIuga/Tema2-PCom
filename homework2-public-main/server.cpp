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
    setvbuf(stdout, nullptr, _IONBF, BUFSIZ);
    DIE(argc != 2, "Usage: ./server <PORT_NUMBER>");

    int socket_TCP, socket_UDP, client_socket_fd, portno;
    char buffer[BUF_LEN];
    struct sockaddr_in serv_addr{}, cli_addr{}, udp_addr{};
    int i;
    ssize_t ret;
    socklen_t tcp_len, udp_len;

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
    DIE(socket_TCP < 0, "Error when creating the TCP socket.");

    // disable Nagle algorithm
    int nagle = 1;
    DIE(setsockopt(socket_TCP, IPPROTO_TCP, TCP_NODELAY, &nagle, sizeof(nagle)) < 0,
        "Error when disabling the Nagle algorithm.");

    portno = atoi(argv[1]);
    DIE(portno == 0, "Error when parsing port using atoi.");

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // bind TCP client socket to port
    DIE(bind(socket_TCP, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0,
        "Error when binding the TCP socket.");

    // enable reuse port action
    int reuse_port = 1;
    DIE(setsockopt(socket_TCP, SOL_SOCKET, SO_REUSEADDR, &reuse_port, sizeof(reuse_port)) < 0,
        "Error when enabling reuse port");

    // listen on the TCP client socket
    DIE(listen(socket_TCP, MAX_CLIENTS) < 0, "Error when listening on the TCP socket.");

    // create UDP socket
    socket_UDP = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(socket_UDP < 0, "Error when creating the UDP socket.");

    // bind UDP client socket to port
    DIE(bind(socket_UDP, (struct sockaddr *) &serv_addr,
             sizeof(struct sockaddr)) < 0, "Error when binding the UDP socket.");

    // Add the sockets for STDIN, the UDP clients and the TCP clients to the read_fds set
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(socket_TCP, &read_fds);
    FD_SET(socket_UDP, &read_fds);
    fdmax = max(socket_TCP, socket_UDP);

    while (true) {
        tmp_fds = read_fds;

        DIE(select(fdmax + 1, &tmp_fds, nullptr, nullptr, nullptr) < 0, "Error when calling select on the server.");

        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
            // read data from STDIN
            memset(buffer, 0, BUF_LEN);
            fgets(buffer, BUF_LEN - 1, stdin);
            buffer[strlen(buffer) - 1] = '\0';

            // check if server received exit command
            if (strncmp(buffer, "exit", 4) == 0 || strlen(buffer) <= 0) {
                // close all clients
                close_clients(map_id_clients, buffer);
                break;
            }

            continue;
        }

        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds)) {
                if (i != socket_UDP) {
                    // A new TCP client wants to connect
                    if (i == socket_TCP) {
                        // accept a connection request from the inactive socket
                        tcp_len = sizeof(cli_addr);
                        client_socket_fd = accept(socket_TCP, (struct sockaddr *) &cli_addr, &tcp_len);
                        DIE(client_socket_fd < 0, "Error when accepting a new TCP client.");

                        // socket is added to the set of read descriptors
                        FD_SET(client_socket_fd, &read_fds);
                        if (client_socket_fd > fdmax) {
                            fdmax = client_socket_fd;
                        }

                        // receive ID from client
                        memset(buffer, 0, BUF_LEN);
                        DIE(recv(client_socket_fd, buffer, sizeof(buffer), 0) < 0, "Error when receiving client ID.");

                        // create connection
                        connect_client(buffer, inet_ntoa(cli_addr.sin_addr),
                                       ntohs(cli_addr.sin_port),
                                       map_connected_clients, map_id_clients,
                                       client_socket_fd);

                        fflush(stdout);
                    } else {
                        // We received a message from a TCP client
                        memset(buffer, 0, BUF_LEN);
                        ssize_t nr_bytes_read = recv(i, buffer, sizeof(buffer), 0);
                        DIE(nr_bytes_read < 0, "Error when reading message from a TCP client.");
                        buffer[strlen(buffer) - 1] = '\0';

                        if (nr_bytes_read == 0 || strncmp(buffer, "exit", 4) == 0) {
                            // client received exit/forceful shut command
                            // client is disconnected from server
                            disconnect_client(i, map_connected_clients);
                            close(i);
                            FD_CLR(i, &read_fds);
                        } else {
                            // process message and execute command
                            execute_tcp_client_command(i, buffer, topics, map_connected_clients);
                        }

                    }
                } else {
                    // We received a message from a UDP client
                    memset(buffer, 0, BUF_LEN);
                    udp_len = sizeof(udp_addr);

                    // receive message from UDP client
                    ret = recvfrom(socket_UDP, buffer, BUF_LEN, 0,
                                   (struct sockaddr *) &udp_addr, &udp_len);
                    DIE(ret < 0, "Error when receiving message from UDP client.");

                    // UDP client was closed so the program moves on
                    if (ret == 0) {
                        continue;
                    }

                    std::string ip(inet_ntoa(udp_addr.sin_addr));
                    int port = ntohs(udp_addr.sin_port);

                    // create a UDP packet
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

    // Close the server's sockets.
    close(socket_UDP);
    close(socket_TCP);

    return 0;
}
