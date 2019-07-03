/*! \file process_request.h
    \brief Implementation of the processing of requests, when they are sent back to the user through the callback functions.
 */
#pragma once

#include "agios_request.h"

struct agios_client {
	void * (* process_request_cb)(int64_t req_id);
	void * (* process_requests_cb)(int64_t *reqs, int32_t reqnb);
};
struct processing_thread_argument_t {
	int64_t *user_ids;
	int32_t reqnb;
};

extern struct agios_client user_callbacks;	

bool process_requests(struct request_t *head_req, int32_t hash);
