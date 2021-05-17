#ifndef NNFS_REQUEST_H
#define NNFS_REQUEST_H

#include "constants.h"

#define REQUEST_MAX_SIZE sizeof(request)

typedef enum {
    CONNECT,
} operation;

typedef struct {
    operation op;
    size_t payload_len;
} request_header;

typedef struct {
    request_header header;
    unsigned char payload[MAX_PAYLOAD_LENGTH];
} request;

#endif //NNFS_REQUEST_H
