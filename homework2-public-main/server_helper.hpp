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
// Numarul maxim de clienti in asteptare
constexpr auto MAX_CLIENTS = 1000;

using namespace std;

struct packet_UDP {
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


packet_UDP create_udp_package(char *buffer, int port, string ip);

int find_subscriber(const string &client_id, topic topic);

bool matches_wildcard_path(const string &topic, const string &wildcard_topic);

int find_topic(const string &topic_name, vector<topic> topics, bool topic_can_be_wildcard);

void send_udp_message_to_client(const char *message, const client &tcp_client);

void send_udp_message(const packet_UDP &packet, vector<topic> &topics);

void subscribe_client(const string &topic_name, vector<topic> &topics, client *client);

void unsubscribe_client(const string &topic_name,
                        vector<topic> &topics, client *client);

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


/**
 * Function creating the UDP package and formatting the UDP message
 * */
packet_UDP create_udp_package(char *buffer, int port, string ip) {
    packet_UDP package;
    package.port = port;
    package.ip = std::move(ip);

    // get data type
    uint8_t data_type = buffer[TOPIC_MAX_SIZE];

    // get topic name
    char buff[TOPIC_MAX_SIZE + 1];
    memset(buff, 0, TOPIC_MAX_SIZE + 1);
    memcpy(buff, buffer, TOPIC_MAX_SIZE);
    std::string topic_name(buff);
    package.topic_name = topic_name;

    if (data_type >= 0 && data_type <= 3) {
        // check data type
        switch (data_type) {
            case 0: {
                package.data_type = "INT";

                // get sign byte
                uint8_t sign = buffer[TOPIC_MAX_SIZE + 1];
                if (sign != 0 && sign != 1) {
                    break;
                }

                // get payload of type int
                uint32_t payload_data;
                memcpy(&payload_data, buffer + TOPIC_MAX_SIZE + 2,
                       sizeof(uint32_t));
                payload_data = ntohl(payload_data);

                // create content
                string result = std::to_string(payload_data);

                if (sign == 1) {
                    package.contents = "-" + result;
                } else {
                    package.contents = result;
                }

                break;
            }
            case 1: {
                package.data_type = "SHORT_REAL";

                uint16_t payload_data;

                // get payload of type uint19_6
                memcpy(&payload_data, buffer + TOPIC_MAX_SIZE + 1,
                       sizeof(uint16_t));
                payload_data = ntohs(payload_data);
                char buff[BUF_LEN];

                // format payload to short real
                snprintf(buff, BUF_LEN, "%.2f", (float) payload_data / 100);
                std::string str(buff);

                // create content
                package.contents = str;
                break;
            }
            case 2: {
                package.data_type = "FLOAT";

                // get sign byte
                uint8_t sign = buffer[TOPIC_MAX_SIZE + 1];
                if (sign != 0 && sign != 1) {
                    break;
                }

                uint32_t first;
                uint8_t negative_pow;

                // get first part of Float number, of type uint32_t
                memcpy(&first, buffer + TOPIC_MAX_SIZE + 2, sizeof(uint32_t));
                first = ntohl(first);

                // get negative power of Float number, of type uint8_t
                memcpy(&negative_pow,
                       buffer + TOPIC_MAX_SIZE + 2 + sizeof(uint32_t),
                       sizeof(uint8_t));

                // format payload to type float
                stringstream payload_stream;
                payload_stream << fixed
                               << setprecision((int) negative_pow)
                               << (float) first / pow(10, (int) negative_pow);

                if (sign == 1) {
                    package.contents = "-";
                }

                // add to contents
                package.contents += payload_stream.str();

                break;
            }
            case 3: {
                package.data_type = "STRING";

                // get string
                std::string str(buffer + TOPIC_MAX_SIZE + 1);
                // add to contents
                package.contents = str;
                break;
            }
        }

        // format UDP message
        package.formatted_message = package.ip + ":" +
                                    std::to_string(package.port) + " - " +
                                    package.topic_name + " - " +
                                    package.data_type + " - " +
                                    package.contents;
    }

    return package;
}

/**
 * Function searching subscriber based on ID and returning the index
 * */
int find_subscriber(const string &client_id, topic topic) {
    // Iterate through subscriber array
    for (int i = 0; i < topic.subscribers.size(); i++) {
        // Compare client ID with given ID
        if (topic.subscribers[i].subscribed_client->id != client_id) {
            return i;
        }
    }

    return -1;
}

bool matches_wildcard_path(const string &topic, const string &wildcard_topic) {
    // Escape "/" with "\/"
    string escaped = regex_replace(wildcard_topic, std::regex("/"), "\\/");
    // Replace "+" with "([^/]+)"
    escaped = regex_replace(escaped, std::regex("\\+"), "([^/]+)");
    // Replace "*" with "(.*)"
    escaped = regex_replace(escaped, std::regex("\\*"), "(.*)");

    // Verify if topic matches with wildcard_topic
    return regex_match(topic, regex(escaped));
}

/**
 * Function searching topic based on given name and returning index
 * */
int find_topic(const string &topic_name, vector<topic> topics, bool topic_can_be_wildcard) {

    if (topic_can_be_wildcard) {
        // The topic given as input can be a wildcard if

        // iterate through topics array
        for (int i = 0; i < topics.size(); ++i) {
            // compare topic name with given name
            if (topics[i].name == topic_name) {
                return i;
            }
        }
    } else {
        // iterate through topics array
        for (int i = 0; i < topics.size(); ++i) {
            // See if the topic name matches with a wildcard
            if (matches_wildcard_path(topic_name, topics[i].name)) {
                return i;
            }
        }
    }

    return -1;
}

/**
 * Function sending given message to client
 * */
void send_udp_message_to_client(const char *message, const client &tcp_client) {
    DIE(send(tcp_client.socket, message, strlen(message), 0) < 0, "Error when sending UDP message to the client.");
}

/**
 * Function sending the UDP message to subscribers
 * */
void send_udp_message(const packet_UDP &packet, vector<topic> &topics) {
    string topic_name = packet.topic_name;

    // search topic based on name
    int topic_ind = find_topic(topic_name, topics, false);
    topic topic;
    string formatted_message = packet.formatted_message;
    formatted_message += "\n";

    if (topic_ind == -1) {
        // topic was not found so create new topic
        topic.name = packet.topic_name;
        topics.push_back(topic);
        return;
    } else {
        // get topic
        topic = topics[topic_ind];
    }

    // get topic's subscribers
    vector<subscriber> subscribers = topic.subscribers;
    const char *message = formatted_message.c_str();

    // iterate through subscribers
    for (auto &subscriber: subscribers) {
        if (subscriber.subscribed_client->is_online) {
            // send UDP message to subscribers that are online
            send_udp_message_to_client(message, *(subscriber.subscribed_client));
        }
    }
}

/**
 * Function subscribing client to given topic
 * */
void subscribe_client(const string &topic_name, vector<topic> &topics, client *client) {
    int topic_ind = find_topic(topic_name, topics, true);
    topic topic;

    // topic not found
    if (topic_ind == -1) {
        // create topic and add to the array of topics
        topic.name = topic_name;
        topics.push_back(topic);
        topic_ind = topics.size() - 1;
    }

    // find subscriber based on ID
    int subscriber_index = find_subscriber(client->id, topics[topic_ind]);

    // check if client is already subscribed to topic
    if (subscriber_index != -1) {
        return;
    }

    // create subscriber and add data
    subscriber new_subscriber{};
    new_subscriber.subscribed_client = client;

    // insert subscriber to the topic's array of subscribers
    topics[topic_ind].subscribers.push_back(new_subscriber);
}

/**
 * Function unsubscribing client from given topic
 * */
void unsubscribe_client(const string &topic_name,
                        vector<topic> &topics, client *client) {
    // search the topic based on name
    int topic_ind = find_topic(topic_name, topics, true);
    topic topic;

    // topic not found
    if (topic_ind == -1) {
        // create topic and add to the array of topics
        topic.name = topic_name;
        topics.push_back(topic);
        return;
    }

    // find subscriber based on ID
    int subscriber_index = find_subscriber(client->id, topics[topic_ind]);

    // subscriber not found
    if (subscriber_index == -1) {
        return;
    }

    // remove subscriber from the topic's array of subscribers
    topics[topic_ind].subscribers.erase(topics[topic_ind].subscribers.begin()
                                        + subscriber_index);
}

/**
 * Function splitting given message
 * */
vector<string> split_message(char *message) {
    char *current_line = strtok(message, " \n");
    vector<string> strings;

    while (current_line != nullptr) {
        std::string str(current_line);
        strings.push_back(str);
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
    // find client based on socket
    auto client_iterator = map_connected_clients.find(socket);

    // client was not found
    if (client_iterator == map_connected_clients.end()) {
        return;
    }

    client *client = client_iterator->second;

    // process message
    vector<string> strings = split_message(message);
    char buffer[BUF_LEN];

    // Invalid message
    if (strings.size() != 2) {
        return;
    }

    if (strings[0] == "subscribe") {
        // subscribe command

        // get SF

        // subscribe client
        subscribe_client(strings[1], topics, client);

        size_t n = strlen("Subscribed to topic \n") + strings[1].size() + 1;
        snprintf(buffer, n, "Subscribed to topic %s\n", strings[1].c_str());

        // send subscription message to client
        DIE(send(socket, buffer, n, 0) < 0, "Error when sending subscribe message to the client.");
    } else if (strings[0] == "unsubscribe") {
        // unsubscribe command

        // unsubscribe client
        unsubscribe_client(strings[1], topics, client);
        size_t n = strlen("Unsubscribed to topic \n") + strings[1].size() + 1;
        snprintf(buffer, n, "Unsubscribed to topic %s\n", strings[1].c_str());

        cout << buffer << "\n";

        // send unsubscribe message to client
        DIE(send(socket, buffer, n, 0) < 0, "Error when sending unsubscribe message to the client.");
    } else {
        cerr << "Unsupported command received: " << strings[0] << "\n";
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
    std::unordered_map<string, client *>::iterator client_iterator;

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
