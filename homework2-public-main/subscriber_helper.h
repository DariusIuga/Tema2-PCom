#ifndef TEMA2_PCOM_SUBSCRIBER_HELPER_H
#define TEMA2_PCOM_SUBSCRIBER_HELPER_H

#include <bits/stdc++.h>

#include "helper.h"

using namespace std;


/**
 * Function splitting given message into an array of strings
 * */
//int split_message(char *message, vector<string> &tokens) {
//    char buffer[BUF_LEN + 1];
//    memcpy(buffer, message, BUF_LEN);
//    buffer[BUF_LEN] = '\n';
//    string str = buffer;
//    string token;
//    std::stringstream str_stream(str);
//
//    for (int i = 0; i < BUF_LEN; i++) {
//        if (buffer[i] == '\n') {
//            getline(str_stream, token, '\n');
//            tokens.push_back(token);
//        }
//
//        // no partial message
//        if (buffer[i + 1] == '\0') {
//            return 1;
//        }
//    }
//
//    std::getline(str_stream, token, '\n');
//    tokens.push_back(token);
//
//    return 0;
//}

int split_message(char *message, vector<string> &tokens) {
    char buffer[BUF_LEN + 1];
    memcpy(buffer, message, BUF_LEN);
    buffer[BUF_LEN] = '\0'; // null-terminate the buffer
    std::string token;

    for (int i = 0; i <= BUF_LEN; i++) {
        if (buffer[i] == '\n' || buffer[i] == '\0') {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += buffer[i];
        }

        if (buffer[i] == '\0') {
            return 0; // end of message
        }
    }

    return 1; // partial message
}

#endif //TEMA2_PCOM_SUBSCRIBER_HELPER_H
