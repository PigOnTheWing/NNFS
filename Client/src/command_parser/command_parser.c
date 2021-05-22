#include <string.h>

#include "command_parser.h"

static char *args[MAX_ARGS];

static inline void clear_args()
{
    size_t  i;
    for (i = 0; i < MAX_ARGS; ++i) {
        args[i] = NULL;
    }
}

char *get_arg(size_t arg_i)
{
    return args[arg_i];
}

static inline int make_cd_request(const operation op, char *tail, request *r)
{
    size_t tail_len;
    r->header.op = op;
    r->header.payload_len = 0;

    if (tail != NULL) {
        tail_len = strlen(tail);
        if (tail_len + 1 > MAX_PAYLOAD_LENGTH) {
            return -1;
        }

        args[CD_DIR] = tail;

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

static inline int make_read_request(char *tail, request *r)
{
    size_t from_len;
    char *space_ptr, *from, *to;

    if (tail == NULL) {
        return -1;
    }

    if ((space_ptr = strchr(tail, ' ')) == NULL) {
        return -1;
    }

    *space_ptr = 0;
    from = tail;
    to = space_ptr + 1;

    from_len = strlen(from);
    if (from_len + 1 > MAX_PAYLOAD_LENGTH) {
        return -1;
    }

    args[READ_SRC] = from;
    args[READ_DEST] = to;

    r->header.op = CMD_READ;
    r->header.payload_len = from_len + 1;
    strcpy((char *) r->payload, from);

    return 0;
}

int parse_command(char *command, request *r)
{
    clear_args();

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
    } else if (!strcmp(command, "read")) {
        return make_read_request(tail, r);
    }

    return -1;
}
