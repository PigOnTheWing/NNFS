#include <malloc.h>
#include <string.h>

#include "session.h"

static size_t session_id_gen = 1;

static struct  {
    size_t count;
    session *head;
    session *tail;
} session_list = { .count = 0, .head = NULL, .tail = NULL };

size_t create_session(int client_fd)
{
    session *s;

    s = (session *) malloc(sizeof(session));
    if (s == NULL) {
        return 0;
    }

    s->session_id = session_id_gen++;
    s->client_fd = client_fd;
    s->curr_dir = NULL;
    s->fp = NULL;
    s->rw_filename = NULL;
    s->next = NULL;

    if (session_list.count == 0) {
        s->prev = NULL;

        session_list.head = s;
        session_list.tail = s;
    } else {
        s->prev = session_list.tail;
        session_list.tail->next = s;
        session_list.tail = s;
    }

    ++session_list.count;
    return s->session_id;
}

const session *get_session(const size_t session_id)
{
    session *s;
    for (s = session_list.head; s != NULL; s = s->next) {
        if (s->session_id == session_id) {
            return s;
        }
    }
    return s;
}

const session *get_session_by_fd(int client_fd)
{
    session *s;
    for (s = session_list.head; s != NULL; s = s->next) {
        if (s->client_fd == client_fd) {
            return s;
        }
    }
    return s;
}

const session *session_change_dir(const size_t session_id, const size_t dir_len, const char *new_dir)
{
    session *s;
    char *dir;
    for (s = session_list.head; s != NULL; s = s->next) {
        if (s->session_id == session_id) {
            dir = (char *) realloc(s->curr_dir, dir_len + 1);
            if (dir == NULL) {
                free(s->curr_dir);
                s->curr_dir = NULL;
                return s;
            }

            strcpy(dir, new_dir);
            s->curr_dir = dir;
            return s;
        }
    }
    return s;
}

const session *session_set_fp(const size_t session_id, FILE *fp)
{
    session *s;
    for (s = session_list.head; s != NULL; s = s->next) {
        if (s->session_id == session_id) {
            s->fp = fp;
            return s;
        }
    }
    return s;
}

const session *session_set_filename(size_t session_id, char *filename)
{
    session *s;
    for (s = session_list.head; s != NULL; s = s->next) {
        if (s->session_id == session_id) {
            s->rw_filename = filename;
            return s;
        }
    }
    return s;
}

const session *session_close_fp(const size_t session_id)
{
    session *s;
    for (s = session_list.head; s != NULL; s = s->next) {
        if (s->session_id == session_id) {
            if (s->fp != NULL) {
                fclose(s->fp);
                s->fp = NULL;
            }

            if (s->rw_filename != NULL) {
                free(s->rw_filename);
                s->rw_filename = NULL;
            }
            return s;
        }
    }
    return s;
}

void close_session(const size_t session_id)
{
    session *s;
    for (s = session_list.head; s != NULL; s = s->next) {
        if (s->session_id == session_id) {
            if (s->curr_dir) {
                free(s->curr_dir);
            }

            if (s->fp != NULL) {
                fclose(s->fp);
            }
            if (s->rw_filename != NULL) {
                free(s->rw_filename);
            }

            --session_list.count;

            if (session_list.head == s && session_list.tail == s) {
                session_list.head = NULL;
                session_list.tail = NULL;
            } else if (session_list.head == s) {
                session_list.head = s->next;
                s->next->prev = NULL;
            } else if (session_list.tail == s) {
                session_list.tail = s->prev;
                s->prev->next = NULL;
            } else {
                s->prev->next = s->next;
                s->next->prev = s->prev;
            }

            free(s);
            s = NULL;
            break;
        }
    }
}

void close_sessions(void)
{
    session *s, *last;

    if (session_list.count >= 1) {
        for (s = session_list.tail->prev; s != NULL; s = s->prev) {
            last = s->next;
            if (last->curr_dir) {
                free(last->curr_dir);
            }

            if (last->fp != NULL) {
                fclose(last->fp);
            }

            if (last->rw_filename != NULL) {
                free(last->rw_filename);
            }

            free(last);
        }
    }

    s = session_list.head;
    free(s);

    session_list.count = 0;
    session_list.head = NULL;
    session_list.tail = NULL;
}
