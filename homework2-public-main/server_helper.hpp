//
// Created by darius on 5/3/24.
//

#ifndef TEMA2_PCOM_SERVER_HELPER_HPP
#define TEMA2_PCOM_SERVER_HELPER_HPP

#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <utility>

#include "helper.hpp"

constexpr auto TOPIC_MAX_SIZE = 50;
// Maximum numbr of clients waiting
constexpr auto MAX_CLIENTS = 1000;

using namespace std;

struct UDP_packet {
    string ip;
    int port{};
    string topic_name;
    string data_type;
    string contents;
    string formatted_message;
};

struct client {
    int socket{};
    string id, ip;
    int port{};
    bool is_online{};
};

struct subscriber {
    client *subscribed_client;
};

struct topic {
    string name;
    vector<subscriber> subscribers;
};


UDP_packet get_udp_packet(char *buffer, string &ip, int port);

int get_subscriber_index(const string &client_id, topic topic);

bool matches_wildcard_path(const string &topic, const string &wildcard_topic);

int get_topic_index(const string &topic_name, vector<topic> topics, bool topic_can_be_wildcard);

void send_udp_packet_to_subscriber(const char *message, const client &tcp_client);

void send_udp_packet(const UDP_packet &packet, vector<topic> &topics);

void subscribe_client(client *client, const string &topic_name, vector<topic> &topics);

void unsubscribe_client(client *client, const string &topic_name,
                        vector<topic> &topics);

vector<string> split_message(char *message);

void execute_tcp_client_command(int socket, char *message,
                                vector<topic> &topics,
                                unordered_map<int, client *> &map_connected_clients);

void close_client(int socket, char buffer[BUF_LEN]);

void close_clients(
        unordered_map<string, client *> map_id_clients,
        char buffer[BUF_LEN]);

void connect_client(const string &id, const string &ip, int port,
                    unordered_map<int, client *> &map_connected_clients,
                    unordered_map<string, client *> &map_id_clients,
                    int socket);

void disconnect_client(int socket,
                       unordered_map<int, client *> &map_connected_clients);


// Create and format UDP message
UDP_packet get_udp_packet(char *buffer, string &ip, int port) {
    UDP_packet package;
    package.port = port;
    package.ip = ip;

    // Get the data type
    uint8_t data_type = buffer[TOPIC_MAX_SIZE];
    // Get the topic name
    char topic_buf[TOPIC_MAX_SIZE + 1];
    memset(topic_buf, 0, TOPIC_MAX_SIZE + 1);
    memcpy(topic_buf, buffer, TOPIC_MAX_SIZE);
    string topic_name(topic_buf);
    package.topic_name = topic_name;

    switch (data_type) {
        case 0: {
            package.data_type = "INT";

            // Get the sign byte
            uint8_t sign = buffer[TOPIC_MAX_SIZE + 1];
            if (!(sign == 0 || sign == 1)) {
                break;
            }

            // Get the int from the buffer
            uint32_t payload_data;
            memcpy(&payload_data, buffer + TOPIC_MAX_SIZE + 2,
                   sizeof(uint32_t));
            // Convert it to host order
            payload_data = ntohl(payload_data);
            string result = to_string(payload_data);

            if (sign == 1) {
                // The int is negative
                package.contents = "-";
            }
            package.contents += result;

            break;
        }
        case 1: {
            package.data_type = "SHORT_REAL";

            uint16_t payload_data;
            memcpy(&payload_data, buffer + TOPIC_MAX_SIZE + 1,
                   sizeof(uint16_t));
            payload_data = ntohs(payload_data);

            char buff[BUF_LEN];
            // Format payload to short real
            snprintf(buff, BUF_LEN, "%.2f", (float) payload_data / 100);
            string str(buff);

            package.contents = str;

            break;
        }
        case 2: {
            package.data_type = "FLOAT";

            // Get the sign byte
            uint8_t sign = buffer[TOPIC_MAX_SIZE + 1];
            if (!(sign == 0 || sign == 1)) {
                break;
            }

            uint32_t mantissa;
            uint8_t negative_exponent;

            // Get mantissa part of the float
            memcpy(&mantissa, buffer + TOPIC_MAX_SIZE + 2, sizeof(uint32_t));
            mantissa = ntohl(mantissa);

            // Get the exponent of the float
            memcpy(&negative_exponent,
                   buffer + TOPIC_MAX_SIZE + 2 + sizeof(uint32_t),
                   sizeof(uint8_t));

            // Format payload to type float
            stringstream payload_data;
            payload_data << fixed << setprecision((int) negative_exponent)
                         << (float) mantissa / pow(10, (int) negative_exponent);

            if (sign == 1) {
                package.contents = "-";
            }
            package.contents += payload_data.str();

            break;
        }
        case 3: {
            package.data_type = "STRING";

            // Get string
            string str(buffer + TOPIC_MAX_SIZE + 1);
            package.contents = str;

            break;
        }
        default: {
            cerr << "Invalid data type " << data_type << " found in UDP package!";
            return package;
        }
    }

    // Format the final UDP message, if the data type was correct
    package.formatted_message = package.ip + ":" +
                                to_string(package.port) + " - " +
                                package.topic_name + " - " +
                                package.data_type + " - " +
                                package.contents;

    return package;
}

// Searches for a subscriber based on ID and returns the index of it in a topic's subscribers
int get_subscriber_index(const string &client_id, topic topic) {
    for (int i = 0; i < topic.subscribers.size(); i++) {
        if (topic.subscribers[i].subscribed_client->id != client_id) {
            return i;
        }
    }

    // The client id wasn't found in the topic's subscriber list
    return -1;
}

// Takes a path topic and checks if it matches with a regex based on a wildcard path
bool matches_wildcard_path(const string &topic, const string &wildcard_topic) {
    // Escape "/" with "\/"
    string escaped = regex_replace(wildcard_topic, regex("/"), "\\/");
    // Replace "+" with "([^/]+)"
    escaped = regex_replace(escaped, regex("\\+"), "([^/]+)");
    // Replace "*" with "(.*)"
    escaped = regex_replace(escaped, regex("\\*"), "(.*)");

    // Verify if topic matches with wildcard_topic
    return regex_match(topic, regex(escaped));
}

// Searches for a topic based on its name in a list of topics and returns its index
int get_topic_index(const string &topic_name, vector<topic> topics, bool topic_can_be_wildcard) {
    // The topic given as input can be a wildcard if we call this function when subscribing or unsubscribing from a topic
    if (topic_can_be_wildcard) {
        for (int i = 0; i < topics.size(); i++) {
            // Compare the name of the topic with the current topic from the vector
            if (topics[i].name == topic_name) {
                return i;
            }
        }
    } else {
        for (int i = 0; i < topics.size(); i++) {
            // See if the topic name matches with a wildcard
            if (matches_wildcard_path(topic_name, topics[i].name)) {
                return i;
            }
        }
    }

    return -1;
}

// Sends a message to a TCP client
void send_udp_packet_to_subscriber(const char *message, const client &tcp_client) {
    DIE(send(tcp_client.socket, message, strlen(message), 0) < 0, "Error when sending UDP message to the client.");
}

// Sends a message to all TCP clients that are subscribed
void send_udp_packet(const UDP_packet &packet, vector<topic> &topics) {
    // Find the index of the topic with the packet's name
    int topic_index = get_topic_index(packet.topic_name, topics, false);
    topic topic;

    if (topic_index == -1) {
        // Topic wasn't found, add it to the topics vector
        topic.name = packet.topic_name;
        topics.push_back(topic);
        return;
    } else {
        // Get the actual topic
        topic = topics[topic_index];
    }

    vector<subscriber> subscribers = topic.subscribers;
    string message_str = packet.formatted_message + "\n";
    const char *message = message_str.c_str();

    for (subscriber &subscriber: subscribers) {
        if (subscriber.subscribed_client->is_online) {
            // Send UDP message to online subscribers
            send_udp_packet_to_subscriber(message, *(subscriber.subscribed_client));
        }
    }
}

// Subscribe a client to a topic
void subscribe_client(client *client, const string &topic_name, vector<topic> &topics) {
    // Search for the topic
    int topic_index = get_topic_index(topic_name, topics, true);

    // Topic was not found
    if (topic_index == -1) {
        // Create a new topic and add it to the vector of topics
        topic topic;
        topic.name = topic_name;
        topics.push_back(topic);
        topic_index = topics.size() - 1;
    }

    // Find subscriber based on their ID
    int sub_index = get_subscriber_index(client->id, topics[topic_index]);

    // Return early if the client was already subscribed to this topic
    if (sub_index != -1) {
        return;
    }

    // Create a nuw subscriber
    subscriber new_subscriber{};
    new_subscriber.subscribed_client = client;
    struct topic current_topic = topics[topic_index];

    // Insert subscriber to the topic's vector of subscribers.
    topics[topic_index].subscribers.push_back(new_subscriber);
}

// Unsubscribe a client from a topic
void unsubscribe_client(client *client, const string &topic_name,
                        vector<topic> &topics) {
    // Search for the topic index
    int topic_index = get_topic_index(topic_name, topics, true);

    // Topic was not found
    if (topic_index == -1) {
        // The topic doesn't exist, do nothing
        return;
    }

    // Find subscriber based on their ID
    int sub_index = get_subscriber_index(client->id, topics[topic_index]);

    // Subscriber not found
    if (sub_index == -1) {
        return;
    }

    // Remove subscriber from the given topic's vector of subscribers
    topics[topic_index].subscribers.erase(topics[topic_index].subscribers.begin()
                                          + sub_index);
}

// Split a message into words
vector<string> split_message(char *message) {
    vector<string> strings;
    char *current_line = strtok(message, " \n");

    while (current_line != nullptr) {
        string line(current_line);
        strings.push_back(line);
        current_line = strtok(nullptr, " ");
    }

    return strings;
}

/**
 * Function processing message from the TCP client and executing the command
 * */
void execute_tcp_client_command(int socket, char *message,
                                vector<topic> &topics,
                                unordered_map<int, client *> &map_connected_clients) {
    // Find client based on sockets
    auto client_iterator = map_connected_clients.find(socket);
    if (client_iterator == map_connected_clients.end()) {
        return;
    }
    client *client = client_iterator->second;

    // Get every word in the message
    vector<string> strings = split_message(message);
    // Invalid message, it should only contain the action and topic
    if (strings.size() != 2) {
        return;
    }

    // Check if the command is wrong
    DIE(!(strings[0] == "subscribe" || strings[0] == "unsubscribe"),
        "Unsupported command received. The only available commands are subscribe and unsubscribe.\n");
    char buffer[BUF_LEN];
    size_t client_message_len;
    if (strings[0] == "subscribe") {
        // Subscribe client
        subscribe_client(client, strings[1], topics);

        client_message_len = strlen("Subscribed to topic \n") + strings[1].size() + 1;
        snprintf(buffer, client_message_len, "Subscribed to topic %s\n", strings[1].c_str());

        // send subscription message to client
        DIE(send(socket, buffer, client_message_len, 0) < 0, "Error when sending subscribe message to the client.");
    } else {
        // Unsubscribe client
        unsubscribe_client(client, strings[1], topics);
        client_message_len = strlen("Unsubscribed to topic \n") + strings[1].size() + 1;
        snprintf(buffer, client_message_len, "Unsubscribed to topic %s\n", strings[1].c_str());

        // send unsubscribe message to client
        DIE(send(socket, buffer, client_message_len, 0) < 0, "Error when sending unsubscribe message to the client.");
    }
}

/**
 * Function closing the connection to a single client
 * */
void close_client(int socket, char buffer[BUF_LEN]) {
    // send the exit message to client
    DIE(send(socket, buffer, strlen(buffer), 0) < 0, "Error when sending unsubscribe message to the client.");

    // close connection
    close(socket);
}

/**
 * Function closing clients connections
 * */
void close_clients(
        unordered_map<string, client *> map_id_clients,
        char buffer[BUF_LEN]) {
    unordered_map<string, client *>::iterator client_iterator;

    // iterate through map of clients
    for (client_iterator = map_id_clients.begin();
         client_iterator != map_id_clients.end(); ++client_iterator) {
        client *current_client = client_iterator->second;

        // check if client is online
        if (current_client->is_online) {
            // close connection to client
            close_client(current_client->socket, buffer);
        }

        // free allocated memory for the client
        delete current_client;
    }
}

/**
 * Function connecting a client from the server
 * */
void connect_client(const string &id, const string &ip, int port,
                    unordered_map<int, client *> &map_connected_clients,
                    unordered_map<string, client *> &map_id_clients,
                    int socket) {
    // search client based on ID
    auto client_iterator = map_id_clients.find(id);

    client *current_client;

    if (client_iterator != map_id_clients.end()) {
        // client with given ID does exist inside the map
        current_client = client_iterator->second;
        if (current_client->is_online) {
            // client is online
            cout << "Client " << id << " already connected.\n";

            if (socket != current_client->socket) {
                // send exit command to the newly opened socket
                char buff[BUF_LEN];
                memset(buff, 0, BUF_LEN);
                memcpy(buff, "exit", strlen("exit"));
                DIE(send(socket, buff, strlen(buff), 0) < 0, "Error when sending exit command to socket.");
            }

            return;
        }

        int previous_socket = current_client->socket;

        // change client information
        current_client->ip = ip;
        current_client->port = port;
        current_client->is_online = true;
        current_client->socket = socket;

        // delete the previous socket key from the map
        map_connected_clients.erase(previous_socket);
        // add the newly created pair to map
        map_connected_clients.insert(pair<int, client *>(socket, current_client));

        map_id_clients.erase(id);
        map_id_clients.insert(pair<string, client *>(id, current_client));

        cout << "New client " << id << " connected from "
             << ip << ":" << port << ".\n";
        return;
    }

    // client was not found so a new one is created
    current_client = new client;

    // add data to the client
    current_client->id = id;
    current_client->ip = ip;
    current_client->port = port;
    current_client->is_online = true;
    current_client->socket = socket;

    // insert a pair of client and socket to the map
    map_connected_clients.insert(pair<int, client *>(socket, current_client));
    map_id_clients.insert(pair<string, client *>(id, current_client));

    cout << "New client " << id << " connected from " << ip << ":" << port << ".\n";
}

/**
 * Function disconnecting a client from the server
 * */
void disconnect_client(int socket,
                       unordered_map<int, client *> &map_connected_clients) {
    // search client based on given socket in map
    auto client_iterator = map_connected_clients.find(socket);

    // client not found
    if (client_iterator == map_connected_clients.end()) {
        return;
    }

    // client was found
    client *current_client = client_iterator->second;
    // client's is_online is changed
    current_client->is_online = false;
    map_connected_clients.erase(socket);

    // Close the fd;
    close(socket);

    cout << "Client " << current_client->id << " disconnected.\n";
}


#endif //TEMA2_PCOM_SERVER_HELPER_HPP
