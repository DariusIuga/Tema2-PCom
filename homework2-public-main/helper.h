#ifndef HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)    \
    if (assertion) {                    \
        fprintf(stderr, "(%s, %d): ",    \
                __FILE__, __LINE__);    \
        perror(call_description);        \
        exit(EXIT_FAILURE);                \
    }                                       \

constexpr auto BUF_LEN = 1560;

#endif
