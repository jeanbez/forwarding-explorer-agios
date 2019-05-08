/*! \file process_request.c
    \brief Implementation of the processing of requests, when they are sent back to the user through the callback functions.
 */
struct user_callbacks; /**< contains the pointers to the user-provided callbacks to be used to process requests */


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
	debug("request - size %ld, offset %ld, file %s - going back to the file system", req->io_data.len, req->io_data.offset, req->file_id);
	req->globalinfo->current_size -= req->io_data.len; //when we aggregate overlapping requests, we don't adjust the related list current_size, since it is simply the sum of all requests sizes. For this reason, we have to subtract all requests from it individually when processing a virtual request.
	req->globalinfo->req_file->timeline_reqnb--;
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
	if ((user_callbacks->process_requests) && (head_req->reqnb > 1)) { //if the user has a function capable of processing a list of requests at once and this is an aggregated request
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
			reqs[reqs_index]=aux_req->data;
			reqs_index++;
		}
		//process
		clnt->process_requests(reqs, head_req->reqnb);
		//we no longer need this array
		free(reqs);
	} else if (head_req->reqnb == 1) {  //this is not a virtual request, just a singleo one
		put_this_request_in_dispatch(head_req, this_time, &head_req->globalinfo->dispatch);
		clnt->process_request(head_req->user_id);
	} else { //this is a virtual request but the user does not have a callback for a list of requests
		agios_list_for_each_entry (req, &head_req->reqs_list, related) { //go through all sub-requests	
			if (aux_req) {
				put_this_request_in_dispatch(aux_req, this_time, &head_req->globalinfo->dispatch);
				clnt->process_request(aux_req->data);
			}
			aux_req = req;
		}
		if(aux_req)
		{
			put_this_request_in_dispatch(aux_req, this_time, &head_req->globalinfo->dispatch);
			clnt->process_request(aux_req->data);
		}
	} //end virtual request but no list callback
	//update requests and files counters
	if (head_req->globalinfo->req_file->timeline_reqnb == 0) dec_current_reqfilenb(); //timeline_reqnb is updated in the put_this_request_in_dispatch function
	dec_many_current_reqnb(hash, head_req->reqnb);
	debug("current status. hashtable[%d] has %d requests, there are %d requests in the scheduler to %d files.", hash, hashlist_reqcounter[hash], current_reqnb, current_reqfilenb); //attention: it could be outdated info since we are not using the lock
	//now check if the scheduling algorithms should stop because it is time to periodic events
	return is_time_to_change_scheduler();
}
