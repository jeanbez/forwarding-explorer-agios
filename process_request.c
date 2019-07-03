/*! \file process_request.c
    \brief Implementation of the processing of requests, when they are sent back to the user through the callback functions.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "agios_counters.h"
#include "agios_request.h"
#include "agios_thread.h"
#include "common_functions.h"
#include "mylist.h"
#include "process_request.h"
#include "req_hashtable.h"
#include "req_timeline.h"
#include "scheduling_algorithms.h"

struct agios_client user_callbacks; /**< contains the pointers to the user-provided callbacks to be used to process requests */


/**
 * unlocks the mutex protecting the data structure where the request is being held. 
 * @param hash If we are using the hashtable, then hash gives the line of the table (because we have one mutex per line). If hash= -1, we are using the timeline (a simple list with only one mutex).
 */
void unlock_structure_mutex(int32_t hash)
{
	if(current_scheduler->needs_hashtable)
		hashtable_unlock(hash);
	else
		timeline_unlock();
}
/** 
 * Locks the mutex protecting the data structure where the request is being held.
 * @param hash If we are using the hashtable, then hash gives the line of the table (because we have one mutex per line). If hash= -1, we are using the timeline (a simple list with only one mutex).
 */
 void lock_structure_mutex(int32_t hash)
{
	if(current_scheduler->needs_hashtable)
		hashtable_lock(hash);
	else
		timeline_lock();
}
/**
 * called when a request is being sent back to the user for processing. It records the timestamp of that happening, and adds the request at the end of a dispatch queue.
 * @param req the request being processed.
 * @param this_time the timestamp of now.
 * @param dispatch the dispatch queue that will receive the request.
 */
void put_this_request_in_dispatch(struct request_t *req, int64_t this_time, struct agios_list_head *dispatch)
{
	agios_list_add_tail(&req->related, dispatch);
	req->dispatch_timestamp = this_time;
	debug("request - size %ld, offset %ld, file %s - going back to the file system", req->len, req->offset, req->file_id);
	req->globalinfo->current_size -= req->len; //when we aggregate overlapping requests, we don't adjust the related list current_size, since it is simply the sum of all requests sizes. For this reason, we have to subtract all requests from it individually when processing a virtual request.
	req->globalinfo->req_file->timeline_reqnb--;
}
/**
 * function executed by the thread that will use the provided callbacks to give requests back to the user. 
 * @param arg is a pointer to a struct processing_thread_argument_t. It will be freed at the end.
 */
void * processing_thread(void *arg)
{
	struct processing_thread_argument_t *info = (struct processing_thread_argument_t *) arg;
	assert(info);
	assert(info->reqnb >= 1);
	if (info->reqnb == 1) { //simplest case, a single request
		user_callbacks.process_request_cb(*info->user_ids);
	} else { //more than one request
		if (NULL != user_callbacks.process_requests_cb) { //we have a callback for a list of requests
			user_callbacks.process_requests_cb(info->user_ids, info->reqnb);
		} else { //we don't have a callback
			for (int32_t i=0; i < info->reqnb; i++) user_callbacks.process_request_cb(info->user_ids[i]);
		}
	}
	free(info->user_ids);
	free(info);
}
/**
 * function called by the process_requests function to create a thread that will use the user callbacks to process requests. That is done separately because if the user callback takes a long time, or uses some mutexes, we don't want that to affect our scheduling algorithm.
 * @param user_ids either a single user_id value (a pointer to it) or a list of them. This is the information provided by the user to agios_add_request to identify the request. If it is a list, we will pass it directly to the user, but if it is a single int64_t we will make a copy of it. Anyway this will be freed by the thread after returning from the callback.
 * @param reqnb the number of user_id values in user_ids (>= 1)
 * @return true or false for success
 */
bool create_processing_thread(int64_t *user_ids, int32_t reqnb)
{
	//first prepare the arguments to the thread
	struct processing_thread_argument_t *arg = (struct processing_thread_argument_t *)malloc(sizeof(struct processing_thread_argument_t));
	if (!arg) return false;
	arg->reqnb = reqnb;
	if (reqnb > 1) { //in this case we already allocated a new list to give it to the user
		arg->user_ids = user_ids;
	} else { //we are using the int64_t that comes from inside the request_t
		arg->user_ids = (int64_t *)malloc(sizeof(int64_t));
		if (!arq->user_ids) return false;
		*(arg->user_ids) = *user_ids;
	}
	//now create the thread
	pthread_t new_thread; /**< used to create the thread, we won't store it because we will never need it. */
	int32_t ret = pthread_create(&new_thread, NULL, processing_thread, (void *)arg);
	if (0 != ret) return false;
	return true;
}
/** 
 * function called by scheduling algorithms to send requests back to the client through the callback functions. The caller must hold relevant data structure mutex (the function will release and then get it again tough). 
 * @param head_req the (possibly virtual) request being processed.
 * @param hash the position of the hashtable where we'll find information about its file.
 * @return true if the scheduling algorithm must stop processing requests and give control back to the agios_thread (because some periodic event is happening), false otherwise.
 */
bool process_requests(struct request_t *head_req, int32_t hash)
{
	struct request_t *req; /**< used to iterate over all requests belonging to this virtual request. */
 	struct request_t *aux_req=NULL; /**< used to avoid removing a request from the virtual request before moving the iterator to the next one, otherwise the loop breaks. */
	struct timespec now;	/**< used to get the dispatch timestamp for the requests. */
	int64_t this_time;	/**< will receive now converted from a struct timespec to a number. */
	
	if (!head_req) return false; /*whaaaat?*/
	//get the timestamp for now
	agios_gettime(&now);
	this_time = get_timespec2long(now);
	if ((user_callbacks.process_requests_cb) && (head_req->reqnb > 1)) { //if the user has a function capable of processing a list of requests at once and this is an aggregated request
		//make a list of requests to be given to the callback function
		//we give to the user an array with the requests' data (the field used by the user, provided in agios_add_request)
		int64_t *reqs = (int64_t *)malloc(sizeof(int64_t)*(head_req->reqnb)); /**< list of requests to be given to the callback function. */
		int32_t reqs_index=0; /**< number of requests in the reqs list while it is being filled. */
		if (!reqs) {
			agios_print("PANIC! Cannot allocate memory for AGIOS.");
			return false;
		}
		agios_list_for_each_entry (req, &head_req->reqs_list, related) { //go through all sub-requests
			if (aux_req) { //we can't just mess with req because the for won't be able to find the next requests after we've modified this one's pointers
				put_this_request_in_dispatch(aux_req, this_time, &head_req->globalinfo->dispatch);
				reqs[reqs_index]=aux_req->user_id;
				reqs_index++;
			}
			aux_req = req;
		}
		if (aux_req) {
			put_this_request_in_dispatch(aux_req, this_time, &head_req->globalinfo->dispatch);
			reqs[reqs_index]=aux_req->user_id;
			reqs_index++;
		}
		//process
		if (!create_processing_thread(reqs, head_req->reqnb)) {
			agios_print("PANIC! Cannot create processing thread.");
			return false;		
		}
	} else if (head_req->reqnb == 1) {  //this is not a virtual request, just a singleo one
		put_this_request_in_dispatch(head_req, this_time, &head_req->globalinfo->dispatch);
		if (!create_processing_thread(head_req->user_id, 1)) {
			agios_print("PANIC! Cannot create processing thread.");
			return false;		
		}
	} else { //this is a virtual request but the user does not have a callback for a list of requests
		agios_list_for_each_entry (req, &head_req->reqs_list, related) { //go through all sub-requests	
			if (aux_req) {
				put_this_request_in_dispatch(aux_req, this_time, &head_req->globalinfo->dispatch);
				user_callbacks.process_request_cb(aux_req->user_id);
			}
			aux_req = req;
		}
		if(aux_req)
		{
			put_this_request_in_dispatch(aux_req, this_time, &head_req->globalinfo->dispatch);
			user_callbacks.process_request_cb(aux_req->user_id);
		}
	} //end virtual request but no list callback
	//update requests and files counters
	if (head_req->globalinfo->req_file->timeline_reqnb == 0) dec_current_filenb(); //timeline_reqnb is updated in the put_this_request_in_dispatch function
	dec_many_current_reqnb(hash, head_req->reqnb);
	debug("current status. hashtable[%d] has %d requests, there are %d requests in the scheduler to %d files.", hash, hashlist_reqcounter[hash], current_reqnb, current_filenb); //attention: it could be outdated info since we are not using the lock
	//now check if the scheduling algorithms should stop because it is time to periodic events
	return is_time_to_change_scheduler();
}
