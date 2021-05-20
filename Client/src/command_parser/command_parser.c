#include <string.h>

#include "command_parser.h"

static inline int make_cd_request(const operation op, const char *tail, request *r)
{
    size_t tail_len;
    r->header.op = op;
    r->header.payload_len = 0;

    if (tail != NULL) {
        tail_len = strlen(tail);
        if (tail_len + 1 > MAX_PAYLOAD_LENGTH) {
            return -1;
        }

        r->header.payload_len = strlen(tail) + 1;
        strcpy((char *) r->payload, tail);
    }

    return 0;
}

static inline int make_ls_request(request *r)
{
    r->header.op = CMD_LIST_CONTENTS;
    r->header.payload_len = 0;
    return 0;
}

int parse_command(char *command, request *r)
{
    size_t command_len;
    char *space_ptr, *tail = NULL;

    command_len = strlen(command);
    if (command[command_len - 1] == '\n') {
        command[command_len - 1] = 0;
    }

    if ((space_ptr = strchr(command, ' '))) {
        *space_ptr = 0;
        tail = space_ptr + 1;
    }

    if (!strcmp(command, "connect")) {
        return make_cd_request(CMD_CONNECT, tail, r);
    } else if (!strcmp(command, "ls")) {
        return make_ls_request(r);
    } else if (!strcmp(command, "cd")) {
        return make_cd_request(CMD_CHANGE_DIR, tail, r);
    }

    return -1;
}
