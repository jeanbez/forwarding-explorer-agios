#pragma once

struct agios_client {
	void * process_request(int64_t req_id);
	void * process_requests(int64_t *reqs, int32_t reqnb);
};

extern struct agios_client user_callbacks;	
short int process_requests(struct request_t *head_req, struct client *clnt, int hash); //returns a flag pointing if some refresh period has expired (scheduling algorithm should exit so we can check it and perform the necessary actions)
