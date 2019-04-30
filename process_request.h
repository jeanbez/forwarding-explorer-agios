#pragma once

short int process_requests(struct request_t *head_req, struct client *clnt, int hash); //returns a flag pointing if some refresh period has expired (scheduling algorithm should exit so we can check it and perform the necessary actions)
