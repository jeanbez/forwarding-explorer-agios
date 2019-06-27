/*! \file NOOP.c
    \brief Implementation of the NOOP scheduling algorithm.
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <string.h>

#include "NOOP.h"

/** 
 * NOOP schedule function. Usually NOOP means not having a schedule function. However, when we dynamically change from another algorithm to NOOP, we may still have requests on queue. So we just process all of them. 
 * @return 0, because we will never decide to sleep 
 */
int64_t NOOP(void *clnt)
{
	struct agios_list_head *list; 
	struct request_t *req;
	bool update_time=0;
	bool stop_processing=false;
	long hash;

	while(!stop_processing) 
	{
		list = timeline_lock(); //we give a change to new requests by locking and unlocking to every rquest. Otherwise agios_add_request would never get the lock.
		stop_processing = agios_list_empty(list);
		if (!stop_processing) { //if the list is not empty
			//just take one request and process it
			req = timeline_oldest_req(&hash);
			debug("NOOP is processing leftover requests %s %ld %ld", req->file_id, req->offset, req->len);
			stop_processing = process_requests(req, clnt, hash);
			generic_post_process(req);
		}
		timeline_unlock();	
	}
	return 0;
}

