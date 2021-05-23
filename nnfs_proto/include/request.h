#ifndef NNFS_REQUEST_H
#define NNFS_REQUEST_H

#include "constants.h"

#define REQUEST_MAX_SIZE sizeof(request)

typedef enum {
    CMD_GET_SESSION,
    CMD_CONNECT,
    CMD_CHANGE_DIR,
    CMD_LIST_CONTENTS,
    CMD_CLOSE_SESSION,
    CMD_READ,
    CMD_READ_NEXT,
    CMD_WRITE,
    CMD_WRITE_PART,
    CMD_WRITE_LAST,
    CMD_WRITE_CANCEL,
} operation;

typedef struct {
    size_t session_id;
    operation op;
    size_t payload_len;
} request_header;

typedef struct {
    request_header header;
    unsigned char payload[MAX_PAYLOAD_LENGTH];
} request;

#endif //NNFS_REQUEST_H
