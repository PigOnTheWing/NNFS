#include <malloc.h>
#include <string.h>

#include "session.h"

static size_t session_id_gen = 1;
static size_t sessions_index = 0;

static session sessions[MAX_SESSIONS];

static const session empty_session = { .session_id = 0, .curr_dir = NULL };

void init_sessions()
{
    size_t i;
    for (i = 0; i < MAX_SESSIONS; ++i) {
        sessions[i] = empty_session;
    }
}

size_t create_session()
{
    if (sessions_index == MAX_SESSIONS) {
        return 0;
    }

    size_t session_id = session_id_gen++;
    session s = {
            .session_id = session_id,
            .curr_dir = NULL
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

void close_session(const size_t session_id)
{
    size_t i, j;
    for (i = 0; i < sessions_index; ++i) {
        if (sessions[i].session_id == session_id) {
            if (sessions[i].curr_dir) {
                free(sessions[i].curr_dir);
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
        sessions[i] = empty_session;
    }
    sessions_index = 0;
}
