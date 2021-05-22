#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "request_dispatcher.h"

static const char default_dir[] = "/home";

static char *get_real_path(const char *path_start, const char *path_end)
{
    size_t concat_path_size;
    char *concat_path = NULL, *full_path;

    concat_path_size = strlen(path_start) + 1 + strlen(path_end) + 1;                      // "/curr/dir" + '/' + "../rel/dir"
    if ((concat_path = malloc(concat_path_size)) == NULL) {
        return NULL;
    }

    strcpy(concat_path, path_start);
    strcat(concat_path, "/");
    strcat(concat_path, path_end);

    full_path = realpath(concat_path, NULL);

    free(concat_path);
    return full_path;
}

static void handle_default_change_dir(const session *s, response *resp)
{
    DIR *dir;
    const size_t default_dir_len = strlen(default_dir);

    resp->header.session_id = s->session_id;

    if ((dir = opendir(default_dir)) == NULL) {
        resp->header.code = OP_FAILED;
        goto fail;
    }

    s = session_change_dir(s->session_id, default_dir_len, default_dir);
    if (s->curr_dir == NULL) {
        resp->header.code = OP_FAILED;
        goto fail;
    }

    resp->header.code = OP_DEFAULT;
    resp->header.payload_len = default_dir_len + 1;
    strcpy((char *) resp->payload, default_dir);
    closedir(dir);
    return;

    fail:
    resp->header.payload_len = 0;
    closedir(dir);
}

static void handle_connect(const session *s, request *req, response *resp)
{
    size_t path_len;
    char *dir_path, *full_path;
    DIR *dir;

    if (req->header.payload_len == 0) {
        handle_default_change_dir(s, resp);
        return;
    }

    dir_path = (char *) req->payload;
    if (dir_path[req->header.payload_len - 1] != 0) {
        dir_path[req->header.payload_len] = 0;
    }

    if ((dir = opendir(dir_path)) == NULL) {
        if (errno == EACCES) {
            resp->header.code = OP_ACCESS_DENIED;
        } else {
            resp->header.code = OP_FAILED;
        }
        goto finish_resp;
    }

    if ((full_path = realpath(dir_path, NULL)) == NULL) {
        resp->header.code = OP_FAILED;
        goto finish_resp;
    }

    path_len = strlen(full_path);
    s = session_change_dir(s->session_id, path_len, full_path);
    if (s->curr_dir == NULL) {
        resp->header.code = OP_FAILED;
        goto finish_resp;
    }

    free(full_path);

    resp->header.code = OP_OK;
    finish_resp:
    resp->header.session_id = s->session_id;
    resp->header.payload_len = 0;
    closedir(dir);
}

static void handle_change_dir(const session *s, request *req, response *resp)
{
    size_t path_len;
    char *rel_path, *full_path = NULL;
    DIR *dir;

    if (req->header.payload_len == 0) {
        handle_default_change_dir(s, resp);
        return;
    }

    rel_path = (char *)req->payload;
    if (rel_path[0] == '/') {
        handle_connect(s, req, resp);
        return;
    }

    if (rel_path[req->header.payload_len - 1] != 0) {
        rel_path[req->header.payload_len] = 0;
    }

    full_path = get_real_path(s->curr_dir, rel_path);
    if (full_path == NULL) {
        resp->header.code = OP_FAILED;
        goto finish_resp;
    }

    if ((dir = opendir(full_path)) == NULL) {
        if (errno == EACCES) {
            resp->header.code = OP_ACCESS_DENIED;
        } else {
            resp->header.code = OP_FAILED;
        }
        goto finish_resp;
    }

    path_len = strlen(full_path);
    s = session_change_dir(s->session_id, path_len, full_path);
    if (s->curr_dir == NULL) {
        resp->header.code = OP_FAILED;
        goto finish_resp;
    }

    closedir(dir);

    resp->header.code = OP_OK;
    finish_resp:
    resp->header.session_id = s->session_id;
    resp->header.payload_len = 0;
    free(full_path);
}

static void handle_list_dir_contents(const session *s, response *resp)
{
    size_t content_len = 0, entry_len;
    DIR *dir;
    struct dirent *dir_content;

    if (s->curr_dir == NULL) {
        resp->header.code = OP_FAILED;
        resp->header.session_id = s->session_id;
        resp->header.payload_len = 0;
        return;
    }

    resp->payload[0] = 0;

    dir = opendir(s->curr_dir);                                                                 // No checks, because curr_dir was checked when it was set
    while ((dir_content = readdir(dir)) != NULL) {
        entry_len = strlen(dir_content->d_name);
        if (content_len + entry_len + 1 + 1 > MAX_PAYLOAD_LENGTH) {                             // content_len + entry_len + '\n' + '\0'
            break;
        }

        content_len += entry_len + 1;

        strcat((char *) resp->payload, dir_content->d_name);
        strcat((char *) resp->payload, "\n");
    }

    resp->header.code = OP_OK;
    resp->header.session_id = s->session_id;
    resp->header.payload_len = content_len + 1;
}

static void handle_close_session(const session *s, response *resp)
{
    close_session(s->session_id);
    resp->header.code = OP_OK;
    resp->header.session_id = 0;
    resp->header.payload_len = 0;
}

static void handle_read_init(const session *s, request *req, response *resp)
{
    size_t full_path_size;
    char *full_path, *rel_path;
    struct stat file_stat;
    FILE *fp;

    rel_path = (char *) req->payload;
    if (rel_path[req->header.payload_len - 1] != 0) {
        rel_path[req->header.payload_len] = 0;
    }

    if (rel_path[0] == '/') {
        full_path_size = strlen(rel_path) + 1;
        full_path = malloc(full_path_size);
        if (full_path ==NULL) {
            resp->header.code = OP_FAILED;
            goto fail;
        }

        strcpy(full_path, rel_path);
    } else {
        full_path = get_real_path(s->curr_dir, rel_path);
        if (full_path ==NULL) {
            resp->header.code = OP_FAILED;
            goto fail;
        }
    }

    if (stat(full_path, &file_stat) || (fp = fopen(full_path, "rb")) == NULL) {
        if (errno == EACCES) {
            resp->header.code = OP_ACCESS_DENIED;
        } else {
            resp->header.code = OP_FAILED;
        }
        goto fail;
    }

    s = session_set_fp(s->session_id, fp);

    resp->header.code = OP_OK;
    resp->header.session_id = s->session_id;
    resp->header.payload_len = sizeof(file_stat.st_size);
    memcpy(resp->payload, &file_stat.st_size, sizeof(file_stat.st_size));
    free(full_path);
    return;

    fail:
    resp->header.session_id = s->session_id;
    resp->header.payload_len = 0;
    free(full_path);
}

static void handle_read_next(const session *s, response *resp)
{
    size_t bytes_read;
    FILE *fp;

    if (s->fp == NULL) {
        resp->header.code = OP_FAILED;
        goto fail;
    }

    fp = s->fp;
    bytes_read = fread(resp->payload, 1, MAX_PAYLOAD_LENGTH, fp);
    if (bytes_read != MAX_PAYLOAD_LENGTH) {
        if (!feof(fp)) {
            resp->header.code = OP_FAILED;
            goto fail;
        }

        s = session_close_fp(s->session_id);
        fp = NULL;

        resp->header.code = OP_LAST;
    } else {
        s = session_set_fp(s->session_id, fp);
        resp->header.code = OP_PART;
    }

    resp->header.session_id = s->session_id;
    resp->header.payload_len = bytes_read;
    return;

    fail:
    resp->header.session_id = s->session_id;
    resp->header.payload_len = 0;
}

const session *get_client_session(int client_fd, const request *req)
{
    if (req->header.session_id > 0) {
        return get_session(req->header.session_id);
    }

    return get_session_by_fd(client_fd);
}

void dispatch_request(const session *s, request *req, response *resp)
{
    switch (req->header.op) {
        case CMD_GET_SESSION: {
            resp->header.code = OP_OK;
            resp->header.session_id = s->session_id;
            resp->header.payload_len = 0;
            break;
        }
        case CMD_CONNECT:
            handle_connect(s, req, resp);
            break;
        case CMD_CHANGE_DIR:
            handle_change_dir(s, req, resp);
            break;
        case CMD_LIST_CONTENTS:
            handle_list_dir_contents(s, resp);
            break;
        case CMD_CLOSE_SESSION:
            handle_close_session(s, resp);
            break;
        case CMD_READ:
            handle_read_init(s, req, resp);
            break;
        case CMD_READ_NEXT:
            handle_read_next(s, resp);
            break;
        default: {
            resp->header.session_id = s->session_id;
            resp->header.code = OP_NOTFOUND;
            resp->header.payload_len = 0;
        }
    }
}
