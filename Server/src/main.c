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

#include "request_dispatcher/request_dispatcher.h"

static const size_t BACKLOG = 16;
static const nfds_t MAX_FDS = MAX_SESSIONS + 1;

int get_listening_socket(const char *host, const char *port)
{
    int socket_fd, reuse = 1;
    struct addrinfo hints, *candidates, *candidate;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = 0;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_canonname = NULL;

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
            printf("Failed to set socket options\n");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }

        if (fcntl(socket_fd, F_SETFL, O_NONBLOCK)) {
            printf("Failed to set a socket to be non-blocking\n");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }

        if (bind(socket_fd, candidate->ai_addr, candidate->ai_addrlen)) {
            printf("Failed to bind socket to address\n");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }

        break;
    }

    if (listen(socket_fd, BACKLOG)) {
        printf("Failed to listen for incoming connections\n");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(candidates);

    return socket_fd;
}

int main(int argc, char **argv)
{
    int socket_fd, timeout = 5 * 60 * 1000, status, i, j, len;
    char *colon_ptr, *host, *port;
    bool close_fd = false, compress = false, kill_server = false;
    struct pollfd fds[MAX_FDS];
    int new_fd, fds_size, fds_index = 1;
    unsigned char request_buffer[REQUEST_MAX_SIZE];
    unsigned char *response_buffer;
    size_t buf_len;
    request req;
    response resp;
    const session *s;

    if (argc != 2 || !(colon_ptr = strchr(argv[1], ':'))) {
        printf("Usage: %s host:port\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    *colon_ptr = 0;
    host = argv[1];
    port = colon_ptr + 1;

    socket_fd = get_listening_socket(host, port);

    fds[0].fd = socket_fd;
    fds[0].events = POLLIN;

    while (!kill_server) {
        printf("Awaiting commands...\n");

        status = poll(fds, fds_index, timeout);
        if (status < 0) {
            printf("Failed to wait for connections\n");
            break;
        }

        if (status == 0) {
            printf("Timed out\n");
            break;
        }

        fds_size = fds_index;
        for (i = 0; i < fds_size; ++i) {
            if (fds[i].revents == 0) {
                continue;
            }

            if (fds[i].fd == socket_fd) {
                if (fds[i].revents != POLLIN) {
                    perror(strerror(fds[i].revents));
                    kill_server = true;
                    break;
                }

                printf("Accepting new connections\n");

                do {
                    new_fd = accept(socket_fd, NULL, NULL);
                    if (new_fd < 0) {
                        if (errno != EWOULDBLOCK) {
                            printf("Failed to accept a connection\n");
                            kill_server = true;
                        }
                        break;
                    }

                    if (fcntl(new_fd, F_SETFL, O_NONBLOCK)) {
                        printf("Failed to set a socket to be non-blocking\n");
                        close(new_fd);
                        break;
                    }

                    printf("Creating client session\n");
                    if (!create_session(new_fd)) {
                        printf("Failed to create client session\n");
                        close(new_fd);
                        break;
                    }

                    printf("Accepted a connection, sock_fd = %d\n", new_fd);
                    fds[fds_index].fd = new_fd;
                    fds[fds_index].events = POLLIN;
                    ++fds_index;
                } while (new_fd != -1);
            } else {
                printf("Receiving commands from a client at socket #%d\n", fds[i].fd);
                while (true) {
                    if (fds[i].revents != POLLIN) {
                        printf("Socket closed unexpectedly\n");
                        close_fd = true;
                        break;
                    }

                    len = recv(fds[i].fd, request_buffer, sizeof(request_buffer), 0);
                    if (len < 0) {
                        if (errno != EWOULDBLOCK) {
                            printf("Failed to receive data from client\n");
                            close_fd = true;
                            break;
                        }
                        break;
                    }

                    if (len == 0) {
                        printf("Client closed the connection\n");
                        close_fd = true;
                        break;
                    }

                    if (!decode_request(request_buffer, &req)) {
                        printf("Failed to decode request\n");
                        close_fd = true;
                        break;
                    }

                    s = get_client_session(fds[i].fd, &req);
                    if (s == NULL) {
                        printf("Client session doesn't exit\n");
                        close_fd = true;
                        break;
                    }

                    dispatch_request(s, &req, &resp);
                    buf_len = encode_response(&resp, &response_buffer);
                    if (response_buffer == NULL) {
                        printf("Failed to encode response\n");
                        close_fd = true;
                        break;
                    }

                    len = send(fds[i].fd, response_buffer, buf_len, 0);
                    if (len < 0) {
                        printf("Failed to send response to client\n");
                        close_fd = true;
                        free(response_buffer);
                        break;
                    }
                    free(response_buffer);
                }

                if (close_fd) {
                    if ((s = get_session_by_fd(fds[i].fd)) != NULL) {
                        printf("Closing client session\n");
                        close_session(s->session_id);
                    }

                    printf("Closing client connection\n");
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    close_fd = false;
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
                    --i;
                    --fds_index;
                }
            }
        }
    }

    printf("Closing open connections\n");
    for (i = 0; i < fds_index; ++i) {
        if (fds[i].fd >= 0) {
            close(fds[i].fd);
        }
    }

    printf("Closing sessions");
    close_sessions();

    exit(EXIT_SUCCESS);
}