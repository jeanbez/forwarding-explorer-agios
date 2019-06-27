/*! \file TO.c
    \brief Implementation of the timeorder and timeorder with aggregations scheduling algorithms. Their processing phases is the same, the only difference is in the requests insertion in the queue.
 */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <string.h>

/**
 * repeatedly process the first request of the timeline, until process_requests notify us to stop.
 * @return 0 (because we will never decide to sleep) 
 */
int64_t timeorder(void)
{
	struct request_t *req;	/**< the request we will process. */
	bool TO_stop = false; /**< is it time to stop and go back to the agios thread to do a periodic event? */
	int32_t hash; /**< the hashtable line which contains information about the request we will process. */

	while ((current_reqnb > 0) && (TO_stop == false)) {
		timeline_lock();
		req = timeline_oldest_req(&hash);
		assert(req); //sanity check
		TO_stop = process_requests(req, hash); 
		generic_post_process(req);
		timeline_unlock();
	}
	return 0;
}



