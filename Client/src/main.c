#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>

#include <request_encoder.h>
#include <response_encoder.h>

#include "command_parser/command_parser.h"

#define MAX_COMMAND_LEN 1024

static size_t session_id = 0;

int get_connection_socket(char *host, char *port)
{
    int socket_fd, len;
    size_t request_len;
    unsigned char *request_buffer, response_buffer[RESPONSE_MAX_SIZE];
    struct addrinfo hints, *candidates, *candidate;
    request req;
    response resp;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;

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

    req.header.op = CMD_CREATE_SESSION;
    req.header.session_id = 0;
    req.header.payload_len = 0;

    request_len = encode_request(&req, &request_buffer);
    if (request_buffer == NULL) {
        printf("Failed to encode request\n");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    len = send(socket_fd, request_buffer, request_len, 0);
    if (len < 0) {
        printf("Failed to send a request\n");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    free(request_buffer);

    len = recv(socket_fd, response_buffer, sizeof(response_buffer), 0);
    if (len < 0) {
        goto failed_to_create_session;
    }

    decode_response(response_buffer, &resp);
    if (resp.header.code != OP_OK) {
        failed_to_create_session:
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
            }
            printf("Success\n");
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

int main(int argc, char **argv)
{
    int socket_fd, len;
    size_t request_len;
    char *colon_ptr, *host, *port, command_str[MAX_COMMAND_LEN];
    unsigned char *request_buffer, response_buffer[RESPONSE_MAX_SIZE];
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
            printf("Available commands: connect, cd, ls, quit, help\n"
                   "\tconnect <dir> - set <dir> as a current directory. "
                   "In case <dir> is unspecified, host will set default directory as current\n"
                   "\tcd <dir> - change current directory to <dir>. "
                   "<dir> can be either absolute or relative. "
                   "In case <dir> is unspecified, host will set default directory as current\n"
                   "\tls - list the contents of the current directory.\n"
                   "\tquit - exit the app.\n"
                   "\thelp - print this message.\n");
            continue;
        }

        if (parse_command(command_str, &req)) {
            printf("Failed to parse command\n");
            continue;
        }

        req.header.session_id = session_id;

        request_len = encode_request(&req, &request_buffer);
        if (request_buffer == NULL) {
            printf("Failed to allocate memory for request\nQuitting...\n");
            break;
        }

        len = send(socket_fd, request_buffer, request_len, 0);
        if (len < 0) {
            printf("Failed to send a request\n");
            continue;
        }

        free(request_buffer);

        len = recv(socket_fd, response_buffer, sizeof(response_buffer), 0);
        if (len < 0) {
            printf("Failed to receive a response from server\nQuitting...\n");
            break;
        }

        if (len == 0) {
            printf("Server closed the connection\nQuitting...\n");
            break;
        }

        decode_response(response_buffer, &resp);
        parse_response(&resp);
    }

    close_session(socket_fd);

    close(socket_fd);
    exit(EXIT_SUCCESS);
}