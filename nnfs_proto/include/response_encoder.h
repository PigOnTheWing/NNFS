#ifndef NNFS_RESPONSE_ENCODER_H
#define NNFS_RESPONSE_ENCODER_H

#include "response.h"

size_t encode_pesponse(response *, unsigned char **);
size_t decode_response(unsigned char *, response *);

#endif //NNFS_RESPONSE_ENCODER_H
