#include <malloc.h>
#include <string.h>

#include "../include/request_encoder.h"

static const size_t HEADER_SIZE = sizeof(request_header);

typedef union  {
    request_header header;
    unsigned char header_bytes[HEADER_SIZE];
} header_encoder, header_decoder;

size_t encode_request(request *r, unsigned char **packet_ptr_out)
{
    size_t p_len = r->header.payload_len, packet_len = HEADER_SIZE + p_len;
    header_encoder encoder = { .header = r->header };
    unsigned char *packet_ptr = malloc(packet_len), *payload_ptr;
    memcpy(packet_ptr, encoder.header_bytes, HEADER_SIZE);

    payload_ptr = packet_ptr + HEADER_SIZE;
    memcpy(payload_ptr, r->payload, p_len);
    *packet_ptr_out = packet_ptr;

    return packet_len;
}

size_t decode_request(unsigned char *packet_ptr_in, request *r)
{
    size_t payload_len;
    header_decoder decoder;
    unsigned char *payload_ptr;

    memcpy(decoder.header_bytes, packet_ptr_in, HEADER_SIZE);
    r->header = decoder.header;

    payload_len = r->header.payload_len;
    payload_ptr = packet_ptr_in + HEADER_SIZE;
    memcpy(r->payload, payload_ptr, payload_len);

    return HEADER_SIZE + payload_len;
}
