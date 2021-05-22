#ifndef NNFS_REQUEST_DISPATCHER_H
#define NNFS_REQUEST_DISPATCHER_H

#include "../session/session.h"
#include <request_encoder.h>
#include <response_encoder.h>

const session *get_client_session(int client_fd, const request *req);
void dispatch_request(const session *s, request *req, response *resp);

#endif //NNFS_REQUEST_DISPATCHER_H
