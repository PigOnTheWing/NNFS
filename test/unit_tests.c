#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>

#include <request_encoder.h>
#include <response_encoder.h>

void assert_normal_request()
{
    size_t request_len;
    char *payload = "Request payload\n";
    unsigned char *request_buffer;
    request req, req_decoded;

    req.header.op = CMD_CONNECT;
    req.header.session_id = 0;
    req.header.payload_len = strlen(payload) + 1;
    strcpy((char *) req.payload, payload);

    request_len = encode_request(&req, &request_buffer);
    assert(request_len == sizeof(req.header) + req.header.payload_len);
    assert(request_buffer != NULL);

    decode_request(request_buffer, &req_decoded);
    assert(req.header.op == req_decoded.header.op);
    assert(req.header.session_id == req_decoded.header.session_id);
    assert(req.header.payload_len == req_decoded.header.payload_len);
    assert(strcmp((char *) req.payload, (char *) req_decoded.payload) == 0);

    free(request_buffer);
}

void assert_empty_payload_request()
{
    size_t request_len;
    unsigned char *request_buffer;
    request req, req_decoded;

    req.header.op = CMD_CONNECT;
    req.header.session_id = 0;
    req.header.payload_len = 0;

    request_len = encode_request(&req, &request_buffer);
    assert(request_len == sizeof(req.header) + req.header.payload_len);
    assert(request_buffer != NULL);

    decode_request(request_buffer, &req_decoded);
    assert(req.header.op == req_decoded.header.op);
    assert(req.header.session_id == req_decoded.header.session_id);
    assert(req.header.payload_len == req_decoded.header.payload_len);

    free(request_buffer);
}

void assert_max_length_payload_request()
{
    size_t request_len;
    unsigned char *request_buffer;
    request req, req_decoded;

    req.header.op = CMD_CONNECT;
    req.header.session_id = 0;
    req.header.payload_len = MAX_PAYLOAD_LENGTH;
    memset(req.payload, 0, MAX_PAYLOAD_LENGTH);

    request_len = encode_request(&req, &request_buffer);
    assert(request_len == sizeof(req.header) + req.header.payload_len);
    assert(request_buffer != NULL);

    decode_request(request_buffer, &req_decoded);
    assert(req.header.op == req_decoded.header.op);
    assert(req.header.session_id == req_decoded.header.session_id);
    assert(req.header.payload_len == req_decoded.header.payload_len);
    assert(memcmp(req.payload, req_decoded.payload, MAX_PAYLOAD_LENGTH) == 0);

    free(request_buffer);
}

void assert_overflowing_request()
{
    size_t request_len;
    unsigned char *request_buffer;
    request req;

    req.header.op = CMD_CONNECT;
    req.header.session_id = 0;
    req.header.payload_len = MAX_PAYLOAD_LENGTH + 10;
    memset(req.payload, 0, MAX_PAYLOAD_LENGTH);

    request_len = encode_request(&req, &request_buffer);
    assert(request_len == 0);
    assert(request_buffer == NULL);
}

void assert_normal_response()
{
    size_t response_len;
    char *payload = "Response payload\n";
    unsigned char *response_buffer;
    response resp, resp_decoded;

    resp.header.code = OP_OK;
    resp.header.session_id = 0;
    resp.header.payload_len = strlen(payload) + 1;
    strcpy((char *) resp.payload, payload);

    response_len = encode_response(&resp, &response_buffer);
    assert(response_len == sizeof(resp.header) + resp.header.payload_len);
    assert(response_buffer != NULL);

    decode_response(response_buffer, &resp_decoded);
    assert(resp.header.code == resp_decoded.header.code);
    assert(resp.header.session_id == resp_decoded.header.session_id);
    assert(resp.header.payload_len == resp_decoded.header.payload_len);
    assert(strcmp((char *) resp.payload, (char *) resp_decoded.payload) == 0);

    free(response_buffer);
}

void assert_empty_payload_response()
{
    size_t response_len;
    unsigned char *response_buffer;
    response resp, resp_decoded;

    resp.header.code = OP_OK;
    resp.header.session_id = 0;
    resp.header.payload_len = 0;

    response_len = encode_response(&resp, &response_buffer);
    assert(response_len == sizeof(resp.header) + resp.header.payload_len);
    assert(response_buffer != NULL);

    decode_response(response_buffer, &resp_decoded);
    assert(resp.header.code == resp_decoded.header.code);
    assert(resp.header.session_id == resp_decoded.header.session_id);
    assert(resp.header.payload_len == resp_decoded.header.payload_len);

    free(response_buffer);
}

void assert_max_length_payload_response()
{
    size_t response_len;
    unsigned char *response_buffer;
    response resp, resp_decoded;

    resp.header.code = OP_OK;
    resp.header.session_id = 0;
    resp.header.payload_len = MAX_PAYLOAD_LENGTH;
    memset(resp.payload, 0, MAX_PAYLOAD_LENGTH);

    response_len = encode_response(&resp, &response_buffer);
    assert(response_len == sizeof(resp.header) + resp.header.payload_len);
    assert(response_buffer != NULL);

    decode_response(response_buffer, &resp_decoded);
    assert(resp.header.code == resp_decoded.header.code);
    assert(resp.header.session_id == resp_decoded.header.session_id);
    assert(resp.header.payload_len == resp_decoded.header.payload_len);
    assert(memcmp(resp.payload, resp_decoded.payload, MAX_PAYLOAD_LENGTH) == 0);

    free(response_buffer);
}

void assert_overflowing_response()
{
    size_t response_len;
    unsigned char *response_buffer;
    response resp;

    resp.header.code = OP_OK;
    resp.header.session_id = 0;
    resp.header.payload_len = MAX_PAYLOAD_LENGTH + 10;
    memset(resp.payload, 0, MAX_PAYLOAD_LENGTH);

    response_len = encode_response(&resp, &response_buffer);
    assert(response_len == 0);
    assert(response_buffer == NULL);
}

int main()
{
    assert_normal_request();
    printf("Normal request success\n");

    assert_empty_payload_request();
    printf("Empty payload request success\n");

    assert_max_length_payload_request();
    printf("Max length payload request success\n");

    assert_overflowing_request();
    printf("Overflowing request success\n");

    assert_normal_response();
    printf("Normal response success\n");

    assert_empty_payload_response();
    printf("Empty payload response success\n");

    assert_max_length_payload_response();
    printf("Max length payload response success\n");

    assert_overflowing_response();
    printf("Overflowing response success\n");
}
