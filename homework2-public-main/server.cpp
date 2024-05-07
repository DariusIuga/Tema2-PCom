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

#include "server_helper.hpp"

using namespace std;

int main(int argc, char *argv[]) {
    // Disable stdout buffering
    setvbuf(stdout, nullptr, _IONBF, BUFSIZ);
    // Print correct usage
    DIE(argc != 2, "Usage: ./server <PORT_NUMBER>");

    // The buffer used for receiving messages
    char buffer[BUF_LEN];
    struct sockaddr_in serv_addr{};

    unordered_map<string, client *> map_id_clients;
    unordered_map<int, client *> map_connected_clients;
    vector<topic> topics;

    // Parse port from stdin
    int port_nr = atoi(argv[1]);
    DIE(port_nr == 0, "Error when parsing port using atoi.");

    // Create sockaddr structure
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port_nr);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // Create TCP socket
    int socket_TCP = socket(AF_INET, SOCK_STREAM, 0);
    DIE(socket_TCP < 0, "Error when creating the TCP socket.");
    // Disable the Nagle algorithm
    int nagle = 1;
    DIE(setsockopt(socket_TCP, IPPROTO_TCP, TCP_NODELAY, &nagle, sizeof(nagle)) < 0,
        "Error when disabling the Nagle algorithm.");
    // Bind TCP socket to port
    DIE(bind(socket_TCP, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0,
        "Error when binding the TCP socket.");
    // Mark the main TCP socket as passive and listen for incoming TCP clients on it
    DIE(listen(socket_TCP, MAX_CLIENTS) < 0, "Error when listening on the TCP socket.");

    // Create UDP socket
    int socket_UDP = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(socket_UDP < 0, "Error when creating the UDP socket.");
    // Bind UDP client socket to port
    DIE(bind(socket_UDP, (struct sockaddr *) &serv_addr,
             sizeof(struct sockaddr)) < 0, "Error when binding the UDP socket.");

    // Used by select()
    // Set used for reading
    fd_set read_fds;
    // Set used temporarily
    fd_set temp_fds;
    // Initialize empty sets of read descriptors and temporary set
    FD_ZERO(&read_fds);
    FD_ZERO(&temp_fds);
    // Add the sockets for STDIN, the UDP clients and the TCP clients to the read_fds set
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(socket_TCP, &read_fds);
    FD_SET(socket_UDP, &read_fds);
    int max_nr_fd = max(socket_TCP, socket_UDP);

    while (true) {
        temp_fds = read_fds;

        // temp_fds will contain file descriptors that can be read from
        DIE(select(max_nr_fd + 1, &temp_fds, nullptr, nullptr, nullptr) < 0,
            "Error when calling select on the server.");

        if (FD_ISSET(STDIN_FILENO, &temp_fds)) {
            // Read data from stdin
            memset(buffer, 0, BUF_LEN);
            fgets(buffer, BUF_LEN - 1, stdin);
            buffer[strlen(buffer) - 1] = '\0';

            // Check if the server received exit
            if (strncmp(buffer, "exit", 4) == 0 || strlen(buffer) <= 0) {
                close_clients(map_id_clients, buffer);
                break;
            }

            continue;
        }

        for (int i = 0; i <= max_nr_fd; i++) {
            // Loop through each file descriptor that can be read from
            if (FD_ISSET(i, &temp_fds)) {
                // Clear the previous message from the buffer
                memset(buffer, 0, BUF_LEN);

                if (i == socket_UDP) {
                    // We received a message from a UDP client
                    struct sockaddr_in udp_addr{};
                    socklen_t udp_len = sizeof(udp_addr);

                    // Receive message from UDP client
                    ssize_t nr_bytes_read = recvfrom(socket_UDP, buffer, BUF_LEN, 0,
                                                     (struct sockaddr *) &udp_addr, &udp_len);
                    DIE(nr_bytes_read < 0,
                        "Error when receiving message from UDP client.");

                    // UDP client was closed
                    if (nr_bytes_read == 0) {
                        continue;
                    }

                    // Fetch the client's IP and port from the message
                    std::string ip(inet_ntoa(udp_addr.sin_addr));
                    int port = ntohs(udp_addr.sin_port);

                    // Create a UDP packet based on the buffer contents
                    UDP_packet packet = get_udp_packet(buffer, ip, port);

                    // The packet is formatted incorrectly
                    if (packet.formatted_message.empty()) {
                        continue;
                    }

                    // Send the UDP message to the clients that are subscribed to its topic
                    send_udp_packet(packet, topics);
                } else if (i == socket_TCP) {
                    // A new TCP client wants to connect

                    // Accept a connection request from a new subscriber
                    struct sockaddr_in tcp_address{};
                    socklen_t tcp_len = sizeof(tcp_address);
                    int client_socket_TCP = accept(socket_TCP, (struct sockaddr *) &tcp_address, &tcp_len);
                    DIE(client_socket_TCP < 0, "Error when accepting a new TCP client.");

                    // Add the socket to the set of read fds
                    FD_SET(client_socket_TCP, &read_fds);
                    if (max_nr_fd < client_socket_TCP) {
                        max_nr_fd = client_socket_TCP;
                    }

                    // Receive the client ID
                    DIE(recv(client_socket_TCP, buffer, sizeof(buffer), 0) < 0, "Error when receiving client ID.");

                    // Connect to the new TCP client
                    connect_client(buffer, inet_ntoa(tcp_address.sin_addr),
                                   ntohs(tcp_address.sin_port),
                                   map_connected_clients, map_id_clients,
                                   client_socket_TCP);

                    fflush(stdout);
                } else {
                    // We received a message from a TCP client
                    ssize_t nr_bytes_read = recv(i, buffer, sizeof(buffer), 0);
                    DIE(nr_bytes_read < 0, "Error when reading message from a TCP client.");
                    buffer[strlen(buffer) - 1] = '\0';

                    if (nr_bytes_read == 0 || strncmp(buffer, "exit", 4) == 0) {
                        // Client has received exit/forceful shut down command, disconnect it from the server
                        disconnect_client(i, map_connected_clients);
                        // Remove the fd from the set
                        FD_CLR(i, &read_fds);
                    } else {
                        // Process and execute the command
                        execute_tcp_client_command(i, buffer, topics, map_connected_clients);
                    }
                }
            }
        }
    }


// Close the server's sockets.
    close(socket_UDP);
    close(socket_TCP);

    return 0;
}
