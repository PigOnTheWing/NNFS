#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>

#include <request_encoder.h>
#include <response_encoder.h>

#define BACKLOG 1

static FILE *server_fp = NULL;

void assert_equal_files(const char *f1, const char *f2)
{
    char c1, c2;
    FILE *fp1, *fp2;

    fp1 = fopen(f1, "rb");
    assert(fp1 != NULL);

    fp2 = fopen(f2, "rb");
    assert(fp2 != NULL);

    while (!feof(fp1) || !feof(fp2)) {
        c1 = (char) fgetc(fp1);
        c2 = (char) fgetc(fp2);

        assert(c1 == c2);
    }

    assert(feof(fp1) && feof(fp2));
    fclose(fp1);
    fclose(fp2);
}

int get_listening_socket(const char *host, const char *port)
{
    int status, socket_fd = -1, reuse = 1;
    struct addrinfo hints, *candidates, *candidate;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = 0;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_canonname = NULL;

    status = getaddrinfo(host, port, &hints, &candidates);
    assert(status == 0);

    for (candidate = candidates; candidate; candidate = candidate->ai_next) {
        socket_fd = socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
        if (socket_fd < 0) {
            continue;
        }

        status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        assert(status == 0);

        bind(socket_fd, candidate->ai_addr, candidate->ai_addrlen);
        assert(status == 0);
        break;
    }

    assert(socket_fd > 0);

    status = listen(socket_fd, BACKLOG);
    assert(status == 0);

    freeaddrinfo(candidates);

    return socket_fd;
}

int get_connection_socket(char *host, char *port)
{
    int socket_fd = -1, status;
    struct addrinfo hints, *candidates, *candidate;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = 0;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_canonname = NULL;

    status = getaddrinfo(host, port, &hints, &candidates);
    assert(status == 0);

    for (candidate = candidates; candidate; candidate = candidate->ai_next) {
        socket_fd = socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
        if (socket_fd < 0) {
            continue;
        }

        status = fcntl(socket_fd, F_SETFL, O_NONBLOCK);
        assert(status == 0);

        break;
    }

    assert(socket_fd > 0);

    status = connect(socket_fd, candidate->ai_addr, candidate->ai_addrlen);
    assert(status == 0 || errno == EINPROGRESS);

    freeaddrinfo(candidates);
    return socket_fd;
}

void process_request(int client_socket, int server_socket, request *req) {
    int len;
    size_t request_len;
    unsigned char *client_buffer, server_buffer[REQUEST_MAX_SIZE];

    request_len = encode_request(req, &client_buffer);
    assert(client_buffer != NULL);

    len = send(client_socket, client_buffer, request_len, 0);
    assert(len == request_len);

    free(client_buffer);

    len = recv(server_socket, server_buffer, sizeof(server_buffer), 0);
    assert(len > 0);

    decode_request(server_buffer, req);
}

void process_response(int client_socket, int server_socket, response *resp)
{
    int len;
    size_t response_len;
    unsigned char *server_buffer, client_buffer[RESPONSE_MAX_SIZE];

    response_len = encode_response(resp, &server_buffer);
    assert(server_buffer != NULL);

    len = send(server_socket, server_buffer, response_len, 0);
    assert(len == response_len);

    free(server_buffer);

    len = recv(client_socket, client_buffer, sizeof(client_buffer), 0);
    assert(len > 0);

    decode_response(client_buffer, resp);
}

void assert_read_init(request *req, response *resp)
{
    int status;
    char *filename;
    struct stat file_stat;

    filename = (char *) req->payload;
    status = stat(filename, &file_stat);
    assert(status == 0);

    server_fp = fopen(filename, "rb");
    assert(server_fp != NULL);

    resp->header.code = OP_OK;
    resp->header.session_id = 0;
    resp->header.payload_len = sizeof(file_stat.st_size);
    memcpy(resp->payload, &file_stat.st_size, sizeof(file_stat.st_size));
}

void assert_read_next(response *resp)
{
    size_t bytes_read;

    bytes_read = fread(resp->payload, 1, MAX_PAYLOAD_LENGTH, server_fp);
    if (bytes_read != MAX_PAYLOAD_LENGTH) {
        assert(feof(server_fp));
        resp->header.code = OP_LAST;
        fclose(server_fp);
        server_fp = NULL;
    } else {
        resp->header.code = OP_PART;
    }

    resp->header.session_id = 0;
    resp->header.payload_len = bytes_read;
}

void assert_read(int client_socket, int server_socket)
{
    size_t bytes_written;
    char *filename_client = "client_read.jpg", *filename_server = "server_read.jpg";
    FILE *fp;
    request  req;
    response resp;

    fp = fopen(filename_client, "w");
    assert(fp != NULL);

    req.header.op = CMD_READ;
    req.header.payload_len = strlen(filename_server) + 1;
    strcpy((char *) req.payload, filename_server);

    process_request(client_socket, server_socket, &req);
    assert(req.header.op == CMD_READ);

    assert_read_init(&req, &resp);

    process_response(client_socket, server_socket, &resp);
    assert(resp.header.code == OP_OK);

    req.header.op = CMD_READ_NEXT;
    req.header.session_id = 0;
    req.header.payload_len = 0;

    while (true) {
        process_request(client_socket, server_socket, &req);
        assert(req.header.op == CMD_READ_NEXT);

        assert_read_next(&resp);

        process_response(client_socket, server_socket, &resp);
        assert(resp.header.code == OP_PART || resp.header.code == OP_LAST);

        bytes_written = fwrite(resp.payload, 1, resp.header.payload_len, fp);
        assert(bytes_written == resp.header.payload_len);
        if (resp.header.code == OP_LAST) {
            break;
        }
    }

    fclose(fp);
    assert_equal_files(filename_client, filename_server);
}

void assert_write_init(request *req, response *resp)
{
    char *filename;

    filename = (char *) req->payload;

    server_fp = fopen(filename, "wb");
    assert(server_fp != NULL);

    resp->header.code = OP_OK;
    resp->header.session_id = 0;
    resp->header.payload_len = 0;
}

void assert_write_next(request *req, response *resp)
{
    size_t bytes_written;

    bytes_written = fwrite(req->payload, 1, req->header.payload_len, server_fp);
    assert(bytes_written == req->header.payload_len);

    resp->header.code = OP_OK;
    resp->header.session_id = 0;
    resp->header.payload_len = 0;
}

void assert_write_last(request *req, response *resp)
{
    assert_write_next(req, resp);

    fclose(server_fp);
    server_fp = NULL;
}

void assert_write(int client_socket, int server_socket)
{
    size_t bytes_read;
    char *filename_client = "client_write.jpg", *filename_server = "server_write.jpg";
    FILE *fp;
    request  req;
    response resp;

    fp = fopen(filename_client, "rb");
    assert(fp != NULL);

    req.header.op = CMD_WRITE;
    req.header.session_id = 0;
    req.header.payload_len = strlen(filename_server) + 1;
    strcpy((char *) req.payload, filename_server);

    process_request(client_socket, server_socket, &req);
    assert(req.header.op == CMD_WRITE);

    assert_write_init(&req, &resp);

    process_response(client_socket, server_socket, &resp);
    assert(resp.header.code == OP_OK);

    while (true) {
        bytes_read = fread(req.payload, 1, MAX_PAYLOAD_LENGTH, fp);
        if (bytes_read != MAX_PAYLOAD_LENGTH) {
            assert(feof(fp));
            req.header.op = CMD_WRITE_LAST;
            req.header.payload_len = bytes_read;

            process_request(client_socket, server_socket, &req);
            assert(req.header.op == CMD_WRITE_LAST);

            assert_write_last(&req, &resp);

            process_response(client_socket, server_socket, &resp);
            break;
        } else {
            req.header.op = CMD_WRITE_PART;

            req.header.payload_len = bytes_read;
            process_request(client_socket, server_socket, &req);
            assert(req.header.op == CMD_WRITE_PART);

            assert_write_next(&req, &resp);

            process_response(client_socket, server_socket, &resp);
        }
        assert(resp.header.code == OP_OK);
    }

    fclose(fp);
    assert_equal_files(filename_client, filename_server);
}

int main(void) {
   int status, client_socket, listening_socket, server_socket, socket_err, flags;
   socklen_t socket_err_size = sizeof(socket_err);

   listening_socket = get_listening_socket("localhost", "12345");
   client_socket = get_connection_socket("localhost", "12345");

   server_socket = accept(listening_socket, NULL, NULL);
   assert(server_socket > 0);

   status = getsockopt(client_socket, SOL_SOCKET, SO_ERROR, &socket_err, &socket_err_size);
   assert(status == 0);
   assert(socket_err == 0);

   flags = fcntl(client_socket, F_GETFL);
   assert(flags != -1);

   status = fcntl(client_socket, flags & ~O_NONBLOCK);
   assert(status == 0);

   assert_read(client_socket, server_socket);
   printf("Read successful\n");

   assert_write(client_socket, server_socket);
   printf("Write successful\n");
}