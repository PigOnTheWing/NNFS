#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <request_encoder.h>
#include <response_encoder.h>

static const size_t BACKLOG = 16;
static const nfds_t MAX_FDS = 128;

int main(int argc, char **argv)
{
    int socket_fd = -1, reuse = 1, timeout = 5 * 60 * 1000, status, i, j, len;
    char *colon_ptr, *host, *port;
    bool close_fd = false, compress = false, kill_server = false;
    struct addrinfo hints, *candidates, *candidate;
    struct pollfd fds[MAX_FDS];
    int new_fd, fds_size, fds_index = 1;
    unsigned char request_buffer[REQUEST_MAX_SIZE];
    unsigned char response_buffer[RESPONSE_MAX_SIZE];
    size_t buf_len;
    request req;
    response resp;

    if (argc != 2 || !(colon_ptr = strchr(argv[1], ':'))) {
        printf("Usage: %s host:port\n", argv[0]);
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
        socket_fd = socket(candidate->ai_family, candidate->ai_socktype, 0);
        if (socket_fd < 0) {
            continue;
        }

        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) {
            printf("Failed to set socket options");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }

        if (fcntl(socket_fd, F_SETFD, O_NONBLOCK)) {
            printf("Failed to set a socket to be non-blocking");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }

        if (bind(socket_fd, candidate->ai_addr, candidate->ai_addrlen)) {
            printf("Failed to bind socket to address");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }

        break;
    }

    if (listen(socket_fd, BACKLOG)) {
        printf("Failed to listen for incoming connections");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    fds[0].fd = socket_fd;
    fds[0].events = POLLIN;

    while (!kill_server) {
        printf("Awaiting connections...");

        status = poll(fds, MAX_FDS, timeout);
        if (status < 0) {
            printf("Failed to wait for connections");
            break;
        }

        if (status == 0) {
            printf("Timed out");
            break;
        }

        fds_size = fds_index;
        for (i = 0; i < fds_size; ++i) {
            if (fds[i].revents == 0) {
                continue;
            }

            if (fds[i].revents != POLLIN) {
                perror(strerror(fds[i].revents));
                kill_server = true;
                break;
            }

            if (fds[i].fd == socket_fd) {
                printf("Accepting new connections");

                do {
                    new_fd = accept(socket_fd, NULL, NULL);
                    if (new_fd < 0) {
                        if (new_fd != EWOULDBLOCK) {
                            printf("Failed to accept a connection");
                            kill_server = true;
                            break;
                        }
                    }

                    printf("Accepted connection %d", new_fd);
                    fds[fds_index].fd = new_fd;
                    fds[fds_index].events = POLLIN;
                    ++fds_index;
                } while (new_fd != -1);
            } else {
                while (true) {
                    len = recv(fds[i].fd, request_buffer, sizeof(request_buffer), 0);
                    if (len < 0) {
                        if (len != EWOULDBLOCK) {
                            printf("Failed to receive data from client");
                            close_fd = true;
                            break;
                        }
                        break;
                    }

                    if (len == 0) {
                        printf("Client closed the connection");
                        close_fd = true;
                        break;
                    }

                    decode_request(request_buffer, &req);
                    switch (req.header.op) {
                        case CONNECT: {
                            resp.header.code = OP_OK;
                            resp.header.payload_len = 0;
                            break;
                        }
                        default: {
                            resp.header.code = OP_NOTFOUND;
                            resp.header.payload_len = 0;
                            break;
                        }
                    }

                    buf_len = encode_pesponse(&resp, (unsigned char **) &response_buffer);
                    len = send(fds[i].fd, response_buffer, buf_len, 0);
                    if (len < 0) {
                        printf("Failed to send response to client");
                        close_fd = true;
                        break;
                    }
                }

                if (close_fd) {
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    compress = true;
                }
            }
        }
        if (compress) {
            compress = false;
            for (i = 0; i < fds_index; ++i) {
                if (fds[i].fd == -1) {
                    for (j = i; j < fds_index - 1; ++j) {
                        fds[j] = fds[j + 1];
                    }
                }
                --i;
                --fds_index;
            }
        }
    }

    for (i = 0; i < fds_index; ++i) {
        if (fds[i].fd >= 0) {
            close(fds[i].fd);
        }
    }

    exit(EXIT_SUCCESS);
}