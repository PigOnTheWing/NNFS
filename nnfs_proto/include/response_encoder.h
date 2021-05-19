#ifndef NNFS_RESPONSE_ENCODER_H
#define NNFS_RESPONSE_ENCODER_H

#include "response.h"

size_t encode_response(response *r, unsigned char **packet_ptr_out);
size_t decode_response(unsigned char *, response *);

#endif //NNFS_RESPONSE_ENCODER_H
