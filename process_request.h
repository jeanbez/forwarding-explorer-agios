/*! \file process_request.h
    \brief Implementation of the processing of requests, when they are sent back to the user through the callback functions.
 */
#pragma once

struct agios_client {
	void * process_request(int64_t req_id);
	void * process_requests(int64_t *reqs, int32_t reqnb);
};

extern struct agios_client user_callbacks;	

bool process_requests(struct request_t *head_req, int32_t hash);
