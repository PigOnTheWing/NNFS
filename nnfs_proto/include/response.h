#ifndef NNFS_RESPONSE_H
#define NNFS_RESPONSE_H

#include "constants.h"

#define RESPONSE_MAX_SIZE sizeof(response)

typedef enum {
    OP_OK,
    OP_FAILED,
    OP_NOTFOUND,
    OP_ACCESS_DENIED,
    OP_DEFAULT,
    OP_SESSION_NOTFOUND,
    OP_PART,
    OP_LAST,
} response_code;

typedef struct {
    size_t session_id;
    response_code code;
    size_t payload_len;
} response_header;

typedef struct {
    response_header header;
    unsigned char payload[MAX_PAYLOAD_LENGTH];
} response;

#endif //NNFS_RESPONSE_H
