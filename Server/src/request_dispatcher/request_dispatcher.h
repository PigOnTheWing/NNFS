#ifndef NNFS_REQUEST_DISPATCHER_H
#define NNFS_REQUEST_DISPATCHER_H

#include "../session/session.h"
#include <request_encoder.h>
#include <response_encoder.h>

void dispatch_request(const request *req, response *resp);

#endif //NNFS_REQUEST_DISPATCHER_H
