#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

#include <request_encoder.h>
#include <response_encoder.h>

#define MAX_COMMAND_LEN 256

int main(int argc, char **argv)
{
    int socket_fd, reuse = 1, non_blocking = 1;
    size_t len, request_len;
    char *colon_ptr, *host, *port, command_str[MAX_COMMAND_LEN], *space_ptr;
    unsigned char *request_buffer, response_buffer[RESPONSE_MAX_SIZE];
    bool quit = false;
    request req;
    response resp;
    struct addrinfo hints, *candidates, *candidate;

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
        printf("Failed to find suitable address");
        exit(EXIT_FAILURE);
    }

    for (candidate = candidates; candidate; candidate = candidate->ai_next) {
        socket_fd = socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
        if (socket_fd < 0) {
            continue;
        }

        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) {
            printf("Failed to set socket options");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }

        break;
    }

    if (connect(socket_fd, candidate->ai_addr, candidate->ai_addrlen)) {
        printf("Failed to connect to host");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

//    if (ioctl(socket_fd, FIONBIO, &non_blocking)) {
//        printf("Failed to set a socket to be non-blocking");
//        close(socket_fd);
//        exit(EXIT_FAILURE);
//    }

    freeaddrinfo(candidates);

    while (!quit) {
        printf(">> ");
        fgets(command_str, sizeof(command_str), stdin); //TODO: Move command parsing to a different source file
        len = strlen(command_str);
        if (command_str[len - 1] == '\n') {
            command_str[len - 1] = 0;
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

            len = recv(socket_fd, response_buffer, sizeof(response_buffer), 0);
            if (len < 0) {
                if (errno != EWOULDBLOCK) {
                    printf("Failed to receive a response from server\n");
                }
                break;
            }

            if (len == 0) {
                printf("Server closed the connection\n");
                quit = true;
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
        } else if (!strcmp(command_str, "quit")) {
            quit = true;
            continue;
        } else {
            printf("Unknown command\n");
            continue;
        }
    }

    exit(EXIT_SUCCESS);
}