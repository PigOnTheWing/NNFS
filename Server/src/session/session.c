#include <malloc.h>
#include <string.h>

#include "session.h"

static size_t session_id_gen = 1;
static size_t sessions_index = 0;

static session sessions[MAX_SESSIONS];

static const session empty_session = {
        .session_id = 0,
        .client_fd = -1,
        .curr_dir = NULL,
        .fp = NULL,
        .rw_filename = NULL
};

void init_sessions()
{
    size_t i;
    for (i = 0; i < MAX_SESSIONS; ++i) {
        sessions[i] = empty_session;
    }
}

size_t create_session(int client_id)
{
    if (sessions_index == MAX_SESSIONS) {
        return 0;
    }

    size_t session_id = session_id_gen++;
    session s = {
            .session_id = session_id,
            .client_fd = client_id,
            .curr_dir = NULL,
            .fp = NULL,
            .rw_filename = NULL
    };

    sessions[sessions_index++] = s;
    return session_id;
}

const session *get_session(const size_t session_id)
{
    size_t i;
    for (i = 0; i < sessions_index; ++i) {
        if (sessions[i].session_id == session_id) {
            return &sessions[i];
        }
    }
    return NULL;
}

const session *get_session_by_fd(int client_fd)
{
    size_t i;
    for (i = 0; i < sessions_index; ++i) {
        if (sessions[i].client_fd == client_fd) {
            return &sessions[i];
        }
    }
    return NULL;
}

const session *session_change_dir(const size_t session_id, const size_t dir_len, const char *new_dir)
{
    size_t i;
    char *dir;
    for (i = 0; i < sessions_index; ++i) {
        if (sessions[i].session_id == session_id) {
            dir = (char *) realloc(sessions[i].curr_dir, dir_len + 1);
            if (dir == NULL) {
                sessions[i].curr_dir = NULL;
                return &sessions[i];
            }

            strcpy(dir, new_dir);
            sessions[i].curr_dir = dir;
            return &sessions[i];
        }
    }
    return NULL;
}

const session *session_set_fp(const size_t session_id, FILE *fp)
{
    size_t i;
    for (i = 0; i < sessions_index; ++i) {
        if (sessions[i].session_id == session_id) {
            sessions[i].fp = fp;
            return &sessions[i];
        }
    }
    return NULL;
}

const session *session_set_filename(size_t session_id, char *filename)
{
    size_t i;
    for (i = 0; i < sessions_index; ++i) {
        if (sessions[i].session_id == session_id) {
            sessions[i].rw_filename = filename;
            return &sessions[i];
        }
    }
    return NULL;
}

const session *session_close_fp(const size_t session_id)
{
    size_t i;
    for (i = 0; i < sessions_index; ++i) {
        if (sessions[i].session_id == session_id) {
            if (sessions[i].fp != NULL) {
                fclose(sessions[i].fp);
                sessions[i].fp = NULL;
            }

            if (sessions[i].rw_filename != NULL) {
                free(sessions[i].rw_filename);
                sessions[i].rw_filename = NULL;
            }
            return &sessions[i];
        }
    }
    return NULL;
}

void close_session(const size_t session_id)
{
    size_t i, j;
    for (i = 0; i < sessions_index; ++i) {
        if (sessions[i].session_id == session_id) {
            if (sessions[i].curr_dir) {
                free(sessions[i].curr_dir);
            }

            if (sessions[i].fp != NULL) {
                fclose(sessions[i].fp);
            }
            if (sessions[i].rw_filename != NULL) {
                free(sessions[i].rw_filename);
            }

            for (j = i; j < sessions_index - 1; ++j) {
                sessions[j] = sessions[j + 1];
            }
            sessions[--sessions_index] = empty_session;
            break;
        }
    }
}

void close_sessions()
{
    size_t i;
    for (i = 0; i < sessions_index; ++i) {
        if (sessions[i].curr_dir) {
            free(sessions[i].curr_dir);
        }

        if (sessions[i].fp != NULL) {
            fclose(sessions[i].fp);
        }

        if (sessions[i].rw_filename != NULL) {
            free(sessions[i].rw_filename);
        }
        sessions[i] = empty_session;
    }
    sessions_index = 0;
}
