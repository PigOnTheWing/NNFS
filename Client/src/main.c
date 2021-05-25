#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>

#include <request_encoder.h>
#include <response_encoder.h>

#include "command_parser/command_parser.h"

#define MAX_COMMAND_LEN 1024

static size_t session_id = 0;

int send_and_receive(int socket_fd, request *req, response *resp)
{
    int len;
    size_t request_len;
    unsigned char *request_buffer, response_buffer[RESPONSE_MAX_SIZE];

    request_len = encode_request(req, &request_buffer);
    if (request_buffer == NULL) {
        printf("Failed to allocate memory for request\n");
        return -1;
    }

    len = send(socket_fd, request_buffer, request_len, 0);
    if (len < 0) {
        printf("Failed to send a request\n");
        return -1;
    }

    free(request_buffer);

    len = recv(socket_fd, response_buffer, sizeof(response_buffer), 0);
    if (len < 0) {
        printf("Failed to receive a response\n");
        return -1;
    }

    if (len == 0) {
        printf("Host closed the connection\n");
        return -1;
    }

    decode_response(response_buffer, resp);
    return 0;
}

int get_connection_socket(char *host, char *port)
{
    int socket_fd, status;
    struct addrinfo hints, *candidates, *candidate;
    request req;
    response resp;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = 0;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_canonname = NULL;

    if (getaddrinfo(host, port, &hints, &candidates)) {
        printf("Failed to find suitable address\n");
        exit(EXIT_FAILURE);
    }

    for (candidate = candidates; candidate; candidate = candidate->ai_next) {
        socket_fd = socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
        if (socket_fd < 0) {
            continue;
        }

        break;
    }

    if (connect(socket_fd, candidate->ai_addr, candidate->ai_addrlen)) {
        printf("Failed to connect to host\n");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    req.header.op = CMD_GET_SESSION;
    req.header.session_id = 0;
    req.header.payload_len = 0;

    status = send_and_receive(socket_fd, &req, &resp);
    if (status) {
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    if (resp.header.code != OP_OK) {
        printf("Failed to create a session\n");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    session_id = resp.header.session_id;
    printf("Successfully connected\n");

    freeaddrinfo(candidates);
    return socket_fd;
}

void close_session(int socket_fd)
{
    size_t request_len;
    unsigned char *request_buffer;
    request req;

    req.header.op = CMD_CLOSE_SESSION;
    req.header.session_id = session_id;
    req.header.payload_len = 0;

    request_len = encode_request(&req, &request_buffer);
    if (request_buffer == NULL) {
        return;
    }

    send(socket_fd, request_buffer, request_len, 0);
}

void parse_response(const response *r)
{
    char payload_message[MAX_PAYLOAD_LENGTH];
    switch (r->header.code) {
        case OP_OK:
            if (r->header.payload_len > 0) {
                strncpy(payload_message, (char *)r->payload, r->header.payload_len);
                printf("%s", payload_message);
            } else {
                printf("Success\n");
            }
            break;
        case OP_FAILED:
            printf("Operation failed\n");
            break;
        case OP_DEFAULT:
            strncpy(payload_message, (char *)r->payload, r->header.payload_len);
            printf("Current directory was set to a default directory - %s\n", payload_message);
            break;
        case OP_SESSION_NOTFOUND:
            printf("Client session non existent\n");
            break;
        case OP_ACCESS_DENIED:
            printf("Access denied\n");
            break;
        case OP_NOTFOUND:
            printf("Host doesn't support such an operation\n");
            break;
    }
}

int read_from_server(int socket_fd, request *init_request)
{
    int status;
    size_t chunks, chunk_num = 0, bytes_written;
    char *filename;
    bool remove_file = false;
    __off_t filesize;
    FILE *fp;
    request  req;
    response resp;

    filename = get_arg(RW_DEST);
    if ((fp = fopen(filename, "w")) == NULL) {
        printf("Failed to open file \"%s\"\n", filename);
        return 0;
    }

    status = send_and_receive(socket_fd, init_request, &resp);
    if (status) {
        fclose(fp);
        return -1;
    }

    if (resp.header.code != OP_OK) {
        if (resp.header.code == OP_ACCESS_DENIED) {
            printf("Failed to open file on remote host: Access denied\n");
            fclose(fp);
            return 0;
        }

        printf("Operation failed\n");
        fclose(fp);
        return 0;
    }

    memcpy(&filesize, resp.payload, resp.header.payload_len);
    chunks = filesize % MAX_PAYLOAD_LENGTH == 0
            ? filesize / MAX_PAYLOAD_LENGTH
            : filesize / MAX_PAYLOAD_LENGTH + 1;

    req.header.op = CMD_READ_NEXT;
    req.header.session_id = session_id;
    req.header.payload_len = 0;

    while (true) {
        status = send_and_receive(socket_fd, &req, &resp);
        if (status) {
            remove_file = true;
            break;
        }

        if (resp.header.code != OP_PART && resp.header.code != OP_LAST) {
            printf("Failed to perform read\n");
            remove_file = true;
            break;
        }

        printf("Receiving chunk %zu of %zu\n", ++chunk_num, chunks);
        bytes_written = fwrite(resp.payload, 1, resp.header.payload_len, fp);
        if (bytes_written != resp.header.payload_len) {
            printf("Failed to write to file\n");
            remove_file = true;
            break;
        }

        if (resp.header.code == OP_LAST) {
            printf("Success\nAll chunks saved to \"%s\"\n", filename);
            break;
        }
    }

    fclose(fp);
    if (remove_file) {
        remove(filename);
    }
    return 0;
}

int write_to_server(int socket_fd, request *init_request)
{
    int status;
    size_t chunks, chunk_num = 0, bytes_read;
    char *filename;
    bool finished = false;
    FILE *fp;
    struct stat file_stat;
    request  req;
    response resp;


    filename = get_arg(RW_SRC);
    if (stat(filename, &file_stat)) {
        printf("Failed to get file stat for \"%s\"\n", filename);
        return 0;
    }

    if ((fp = fopen(filename, "rb")) == NULL) {
        printf("Failed to open file \"%s\"\n", filename);
        return 0;
    }

    status = send_and_receive(socket_fd, init_request, &resp);
    if (status) {
        fclose(fp);
        return -1;
    }

    if (resp.header.code != OP_OK) {
        if (resp.header.code == OP_ACCESS_DENIED) {
            printf("Failed to open file on remote host: Access denied\n");
            fclose(fp);
            return 0;
        }

        printf("Operation failed\n");
        fclose(fp);
        return 0;
    }

    chunks = file_stat.st_size % MAX_PAYLOAD_LENGTH == 0
             ? file_stat.st_size / MAX_PAYLOAD_LENGTH
             : file_stat.st_size / MAX_PAYLOAD_LENGTH + 1;

    req.header.session_id = session_id;

    while (!finished) {
        bytes_read = fread(req.payload, 1, MAX_PAYLOAD_LENGTH, fp);
        if (bytes_read != MAX_PAYLOAD_LENGTH) {
            if (feof(fp)) {
                printf("Sending last chunk\n");
                req.header.op = CMD_WRITE_LAST;
            } else {
                printf("Error occurred. Cancelling writing\n");
                req.header.op = CMD_WRITE_CANCEL;
            }
            finished = true;
        } else {
            printf("Sending chunk %zu of %zu\n", ++chunk_num, chunks);
            req.header.op = CMD_WRITE_PART;
        }

        req.header.payload_len = bytes_read;
        status = send_and_receive(socket_fd, &req, &resp);
        if (status) {
            break;
        }

        if (resp.header.code != OP_OK) {
            printf("Failed to write chunk\n");
            break;
        }

        printf("Success\n");
    }

    fclose(fp);
    return 0;
}

int main(int argc, char **argv)
{
    int socket_fd, status;
    char *colon_ptr, *host, *port, command_str[MAX_COMMAND_LEN];
    request req;
    response resp;

    if (argc != 2 || !(colon_ptr = strchr(argv[1], ':'))) {
        printf("Usage: %s server_host:server_port", argv[0]);
        exit(EXIT_SUCCESS);
    }

    *colon_ptr = 0;
    host = argv[1];
    port = colon_ptr + 1;

    socket_fd = get_connection_socket(host, port);

    while (true) {
        printf(">> ");
        fgets(command_str, sizeof(command_str), stdin);
        if (!strncmp(command_str, "quit", 4)) {
            break;
        }

        if (!strncmp(command_str, "help", 4)) {
            printf("Available commands: connect, cd, ls, read, quit, help\n"
                   "\tconnect <dir> - set <dir> as a current directory. "
                   "In case <dir> is unspecified, host will set default directory as current.\n"
                   "\tcd <dir> - change current directory to <dir>. "
                   "<dir> can be either absolute or relative. "
                   "In case <dir> is unspecified, host will set default directory as current.\n"
                   "\tls - list the contents of the current directory.\n"
                   "\tread <src> <dest> - read file <src> from host and save it into file <dest>.\n"
                   "\twrite <src> <dest> - write local file <src> to host under the name <dest>.\n"
                   "\tquit - exit the app.\n"
                   "\thelp - print this message.\n");
            continue;
        }

        if (parse_command(command_str, &req)) {
            printf("Failed to parse command\n");
            continue;
        }

        req.header.session_id = session_id;
        if (req.header.op == CMD_READ) {
            status = read_from_server(socket_fd, &req);
            if (status) {
                break;
            }

            continue;
        }

        if (req.header.op == CMD_WRITE) {
            status = write_to_server(socket_fd, &req);
            if (status) {
                break;
            }

            continue;
        }

        status = send_and_receive(socket_fd, &req, &resp);
        if (status) {
            break;
        }

        parse_response(&resp);
    }

    close_session(socket_fd);

    close(socket_fd);
    exit(EXIT_SUCCESS);
}