#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/poll.h>

#include <request_encoder.h>
#include <response_encoder.h>

#define MAX_COMMAND_LEN 256

int main(int argc, char **argv)
{
    int socket_fd, len, status, timeout = 30 * 1000, socket_err;
    socklen_t socket_err_size = sizeof(socket_err);
    size_t request_len, command_len;
    char *colon_ptr, *host, *port, command_str[MAX_COMMAND_LEN], *space_ptr;
    unsigned char *request_buffer, response_buffer[RESPONSE_MAX_SIZE];
    bool quit = false;
    request req;
    response resp;
    struct addrinfo hints, *candidates, *candidate;
    struct pollfd connection_fd;

    if (argc != 2 || !(colon_ptr = strchr(argv[1], ':'))) {
        printf("Usage: %s server_host:server_port", argv[0]);
        exit(EXIT_SUCCESS);
    }

    *colon_ptr = 0;
    host = argv[1];
    port = colon_ptr + 1;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &candidates)) {
        printf("Failed to find suitable address\n");
        exit(EXIT_FAILURE);
    }

    for (candidate = candidates; candidate; candidate = candidate->ai_next) {
        socket_fd = socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
        if (socket_fd < 0) {
            continue;
        }

        if (fcntl(socket_fd, F_SETFL, O_NONBLOCK)) {
            printf("Failed to set a socket to be non-blocking\n");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }

        break;
    }

    if (connect(socket_fd, candidate->ai_addr, candidate->ai_addrlen)) {
        if (errno != EINPROGRESS) {
            goto failed_to_connect;
        }

        printf("Connecting to host...\n");
        connection_fd.fd = socket_fd;
        connection_fd.events = POLLOUT;

        status = poll(&connection_fd, 1, timeout);
        if (status <= 0) {
            goto failed_to_connect;
        }

        if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &socket_err, &socket_err_size) || socket_err) {
            failed_to_connect:
            printf("Failed to connect to host\n");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }
        printf("Successfully connected\n");
    }

    freeaddrinfo(candidates);

    while (!quit) {
        printf(">> ");
        fgets(command_str, sizeof(command_str), stdin); //TODO: Move command parsing to a different source file
        command_len = strlen(command_str);
        if (command_str[command_len - 1] == '\n') {
            command_str[command_len - 1] = 0;
        }

        if ((space_ptr = strchr(command_str, ' '))) {
            *space_ptr = 0;
        }

        if (!strcmp(command_str, "connect")) {
            req.header.op = CONNECT;
            req.header.payload_len = 0;

            request_len = encode_request(&req, &request_buffer);
            len = send(socket_fd, request_buffer, request_len, 0);
            if (len < 0) {
                printf("Failed to send a request\n");
                continue;
            }

            free(request_buffer);

            while (true) {
                len = recv(socket_fd, response_buffer, sizeof(response_buffer), 0);
                if (len < 0) {
                    if (errno != EWOULDBLOCK) {
                        printf("Failed to receive a response from server\nQuitting...\n");
                        quit = true;
                    }
                    break;
                }

                if (len == 0) {
                    printf("Server closed the connection\nQuitting...\n");
                    quit = true;
                    break;
                }

                decode_response(response_buffer, &resp);
                switch (resp.header.code) {
                    case OP_OK: {
                        printf("Success\n");
                        break;
                    }
                    case OP_NOTFOUND: {
                        printf("Server does not support such operation\n");
                        break;
                    }
                    case OP_FAILED: {
                        printf("Failed\n");
                        break;
                    }
                }
            }
        } else if (!strcmp(command_str, "quit")) {
            quit = true;
            continue;
        } else {
            printf("Unknown command\n");
            continue;
        }
    }

    close(socket_fd);
    exit(EXIT_SUCCESS);
}