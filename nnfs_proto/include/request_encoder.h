#ifndef NNFS_REQUEST_ENCODER_H
#define NNFS_REQUEST_ENCODER_H

#include <stdint.h>

#include "request.h"

size_t encode_request(request *, unsigned char **);
size_t decode_request(unsigned char *, request *);

#endif //NNFS_REQUEST_ENCODER_H
