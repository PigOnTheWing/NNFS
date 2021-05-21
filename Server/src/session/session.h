#ifndef NNFS_SESSION_H
#define NNFS_SESSION_H

#include <stddef.h>

#define MAX_SESSIONS 128

typedef struct {
    size_t session_id;
    int client_fd;
    char *curr_dir;
} session;

void init_sessions();
size_t create_session(int client_fd);
const session *get_session(size_t session_id);
const session *get_session_by_fd(int client_fd);
const session *session_change_dir(size_t session_id, size_t dir_len, const char *new_dir);
void close_session(size_t session_id);
void close_sessions();

#endif //NNFS_SESSION_H
