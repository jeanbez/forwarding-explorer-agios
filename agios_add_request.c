/*! \file agios_add_request.c
    \brief Implementation of the agios_add_request function, used by the user to add requests to AGIOS.

    TODO details
*/  
#include <stdbool.h>
 
#include "agios.h"
#include "agios_request.h"
#include "pattern_tracker.h"

static int32_t g_last_timestamp=0; /**< We increase this number at every new request, just so each one of them has an unique identifier. */

/**
 * Function to allocate and fill a new request struct, used by agios_add_request.
 * @param file_id the handle of the file.
 * @param type is RT_READ or RT_WRITE.
 * @param offset the position of the file being accessed.
 * @param len the size of the request.
 * @param identifier a 64-bit value that makes sense for the user to uniquely identify this request.
 * @param arrival_time the timestamp for the arrival of this request.
 * @param the identifier of the application or server for this request, only relevant for TWINS and SW. @see agios_add_request
 * @see agios_request.h
 * @return the newly allocated and filled request structure, NULL if it failed.
 */
struct request_t * request_constructor(char *file_id, 
					int32_t type, 
					int64_t offset, 
					int64_t len, 
					int64_t identifier,  
					int64_t arrival_time, 
					int32_t queue_id)
{
	struct request_t *new; /**< The new request structure that will be returned */

	//allocate memory
	new = malloc(sizeof(struct request_t));
	if (!new) return NULL;
	new->file_id = malloc(sizeof(char)*(strlen(file_id)+1));
	if (!new->file_id) {
		free(new);
		return NULL;
	}
	//copy the file handle
	strcpy(new->file_id, file_id);
	//fill the structure
	new->queue_id = queue_id;
	new->type = type;
	new->user_id = identifier;
	new->offset = offset;
	new->len = len;
	new->sched_factor = 0;
	new->arrival_time = arrival_time;
	new->reqnb = 1;
	init_agios_list_head(&new->reqs_list);
	new->agg_head=NULL;
	g_last_timestamp++;
	new->timestamp = g_last_timestamp;
	init_agios_list_head(&new->related);
	new->already_waited=0; //TODO see if it is useful
	return new;
}
/** 
 * function called by the user to add a request to AGIOS.
 * @param file_id the file handle associated with the request.
 * @param type is RT_READ or RT_WRITE.
 * @param offset is the position of the file to be accessed (in bytes).
 * @param len is the size of the request (in bytes).
 * @param identifier is a 64-bit value that makes sense for the user to identify this request. It is the argument provided to the callback (so it must uniquely identify this request to the user).
 * @param queue_id is used for the TWINS and SW algorithms to be the identifier of the server or application, respectively. If not relevant, provide 0.
 * @return true of false for success.
 */
bool agios_add_request(char *file_id, 
			int32_t type, 
			int64_t offset, 
			int64_t len, 
			int64_t identifier, 
			int32_t queue_id)
{
	struct request_t *req;  /**< The request structure we will fill with the new request.*/
	struct timespec arrival_time; /**< Filled with the time of arrival for this request */
	int64_t timestamp; /**< It will receive a representation of arrival_time. */
	int32_t hash = get_hashtable_position(file_id); /**< The position of the hashtable where information about this file is, calculated from the file handle. */ 
	bool previous_needs_hashtable; /**< Used to control the used data structure in the case it is being changed while this function is running */

	//build the request_t structure and fill it for the new request, also add it to the current pattern in case we are using the pattern matching mechanism
	agios_gettime(&(arrival_time));
	timestamp = get_timespec2long(arrival_time);
	add_request_to_pattern(timestamp, offset, len, type, file_id); 
	req = request_constructor(file_id, type, offset, len, data, timestamp, queue_id);
	if (!req) return false;
	//acquire the lock for the right data structure (it depends on the current scheduling algorithm being used)
	while (true) { //we'll break out of this loop when we are sure to have acquired the lock for the right data structure
		//check if the current scheduler uses the hashtable or not and then acquire the right lock
		previous_needs_hashtable = current_scheduler->needs_hashtable;
		if (previous_needs_hashtable) hashtable_lock(hash);
		else timeline_lock();
		//the problem is that the scheduling algorithm could have changed while we were waiting to acquire the lock, and then it is possible we have the wrong lock. 
		if (previous_needs_hashtable != current_scheduler->needs_hashtable) {
			//the other thread has migrated scheduling algorithms (and data structure) while we were waiting for the lock (so the lock is no longer the adequate one)
			if (previous_needs_hashtable) hashtable_unlock(hash);
			else timeline_unlock();
		}
		else break; //everything is fine, we got the right lock (and now that we have it, other threads cannot change the scheduling algorithm
	} 
	//add the request to the right data structure
	if (current_scheduler->needs_hashtable) hashtable_add_req(req,hash,NULL);
	else timeline_add_req(req, hash, NULL);
	//update counters and statistics
	hashlist_reqcounter[hash]++;
	req->globalinfo->current_size += req->len;
	req->globalinfo->req_file->timeline_reqnb++;
	statistics_new_req(req);  
	debug("current status: there are %d requests in the scheduler to %d files",current_reqnb, current_reqfilenb);
	//trace this request arrival
	if (config_trace_agios) agios_trace_add_request(req);  
	//increase the number of current requests on the scheduler
	inc_current_reqnb(); 
	// Signalize to the consumer thread that a new request was added. In the case of NOOP scheduler, the agios thread does nothing, we will return the request right away
	if (current_alg != NOOP_SCHEDULER) {
		signal_new_req_to_agios_thread(); 
	} else {
		//if we are running the NOOP scheduler, we just give it back already
		debug("NOOP is directly processing this request");
		process_requests(req, clnt, hash);
		generic_post_process(req);
	}
	//free the lock and leave
	if (previous_needs_hashtable) hashtable_unlock(hash);
	else timeline_unlock();
	return true;
}
