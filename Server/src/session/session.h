#ifndef NNFS_SESSION_H
#define NNFS_SESSION_H

#include <stdio.h>
#include <stddef.h>

#define MAX_SESSIONS 128

typedef struct session {
    size_t session_id;
    int client_fd;
    char *curr_dir;
    FILE *fp;
    char *rw_filename;
    struct session *next;
    struct session *prev;
} session;

size_t create_session(int client_fd);
const session *get_session(size_t session_id);
const session *get_session_by_fd(int client_fd);
const session *session_change_dir(size_t session_id, size_t dir_len, const char *new_dir);
const session *session_set_fp(size_t session_id, FILE *fp);
const session *session_set_filename(size_t session_id, char *filename);
const session *session_close_fp(size_t session_id);
void close_session(size_t session_id);
void close_sessions(void);

#endif //NNFS_SESSION_H
