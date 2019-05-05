
struct user_callbacks; /**< contains the pointers to the user-provided callbacks to be used to process requests */
/***********************************************************************************************************
 * REQUESTS PROCESSING (CALLED BY THE I/O SCHEDULERS THROUGH THE AGIOS THREAD)	   *
 ***********************************************************************************************************/
//unlocks the mutex protecting the data structure where the request is being held.
//if it is the hashtable, then hash gives the line of the table (because we have one mutex per line)
//if hash= -1, we are using the timeline (a simple list with only one mutex)
void unlock_structure_mutex(int32_t hash)
{
	if(current_scheduler->needs_hashtable)
		hashtable_unlock(hash);
	else
		timeline_unlock();
}
//locks the mutex protecting the data structure where the request is being held.
//if it is the hashtable, then hash gives the line of the table (because we have one mutex per line)
//if hash= -1, we are using the timeline (a simple list with only one mutex)
 void lock_structure_mutex(int32_t hash)
{
	if(current_scheduler->needs_hashtable)
		hashtable_lock(hash);
	else
		timeline_lock();
}

void put_this_request_in_dispatch(struct request_t *req, long int this_time, struct agios_list_head *dispatch)
{
	agios_list_add_tail(&req->related, dispatch);
	req->dispatch_timestamp = this_time;
	debug("request - size %ld, offset %ld, file %s - going back to the file system", req->io_data.len, req->io_data.offset, req->file_id);
	req->globalinfo->current_size -= req->io_data.len; //when we aggregate overlapping requests, we don't adjust the related list current_size, since it is simply the sum of all requests sizes. For this reason, we have to subtract all requests from it individually when processing a virtual request.
	req->globalinfo->req_file->timeline_reqnb--;
	
	
}

//must hold relevant data structure mutex (it will release and then get it again tough)
//it will return 1 if some refresh period has expired (it is time to recalculate the alpha factor and redo all predictions, or it is time to change the scheduling algorithm), 0 otherwise
short int process_requests(struct request_t *head_req, struct client *clnt, int hash)
{
	struct request_t *req, *aux_req=NULL;
	short int update_time=0;	
	struct timespec now;
	long int this_time;
	
	if(!head_req)
		return 0; /*whaaaat?*/

	PRINT_FUNCTION_NAME;
	agios_gettime(&now);


	if(config_trace_agios)
		agios_trace_process_requests(head_req);

	if((clnt->process_requests) && (head_req->reqnb > 1)) //if the user has a function capable of processing a list of requests at once and this is an aggregated request
	{
		this_time = get_timespec2long(now);
#ifdef ORANGEFS_AGIOS
		int64_t *reqs = (int64_t *)malloc(sizeof(int64_t)*(head_req->reqnb+1));
#else
		void **reqs = (void **)malloc(sizeof(void *)*(head_req->reqnb+1));
#endif
		int reqs_index=0;
		//we give to the user an array with the requests' data (the field used by the user, provided in agios_add_request)
		agios_list_for_each_entry(req, &head_req->reqs_list, related)
		{
			if(aux_req) //we can't just mess with req because the for won't be able to find the next requests after we've modified this one's pointers
			{
				put_this_request_in_dispatch(aux_req, this_time, &head_req->globalinfo->dispatch);
				reqs[reqs_index]=aux_req->data;
				reqs_index++;
			}
			aux_req = req;
		}
		if(aux_req)
		{
			put_this_request_in_dispatch(aux_req, this_time, &head_req->globalinfo->dispatch);
			reqs[reqs_index]=aux_req->data;
			reqs_index++;
		}
		//update requests and files counters
		if(head_req->globalinfo->req_file->timeline_reqnb == 0)
			dec_current_reqfilenb();
		dec_many_current_reqnb(hash, head_req->reqnb);
		//process
		clnt->process_requests(reqs, head_req->reqnb);
		//we no longer need this array
		free(reqs);
	}
	else{
		if(head_req->reqnb == 1)
		{
			put_this_request_in_dispatch(head_req, get_timespec2long(now), &head_req->globalinfo->dispatch);
			if(head_req->globalinfo->req_file->timeline_reqnb == 0)
				dec_current_reqfilenb();
			dec_current_reqnb(hash);
			clnt->process_request(head_req->data);
		}
		else
		{
			//it is a virtual request but the user did not provide a function to process multiple requests at once, so we'll simply give them sequentially 
			this_time = get_timespec2long(now);
			agios_list_for_each_entry(req, &head_req->reqs_list, related)
			{	
				if(aux_req)
				{
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

			if(head_req->globalinfo->req_file->timeline_reqnb == 0)
				dec_current_reqfilenb();
			dec_many_current_reqnb(hash, head_req->reqnb);
		}
	}

//	if((config_predict_agios_recalculate_alpha_period >= 0) && (agios_processed_reqnb  >= config_predict_agios_recalculate_alpha_period))
//		update_time=1;
//	debug("scheduler is dynamic? %d, algorithm_selection_period = %ld, processed_reqnb = %ld, last_selection_processed_renb = %ld, algorithm_selection_reqnumber = %ld", dynamic_scheduler->is_dynamic, algorithm_selection_period, processed_reqnb, last_selection_processed_reqnb, algorithm_selection_reqnumber);
	if((dynamic_scheduler->is_dynamic) && (config_agios_select_algorithm_period >= 0) && (agios_processed_reqnb >= config_agios_select_algorithm_min_reqnumber))
	{
		if(get_nanoelapsed(last_algorithm_update) >= config_agios_select_algorithm_period)
			update_time=1;
	}


	debug("current status. hashtable[%d] has %d requests, there are %d requests in the scheduler to %d files.", hash, hashlist_reqcounter[hash], current_reqnb, current_reqfilenb); //attention: it could be outdated info since we are not using the lock

	PRINT_FUNCTION_EXIT;
	return update_time;
}
/**
 * Called after processing a request to update some statistics and possibly cleanup a virtual request structure. 
 * @param req the request that was processed.
 */
void generic_post_process(struct request_t *req)
{
	req->globalinfo->lastaggregation = req->reqnb; 
	if (req->reqnb > 1) { //this was an aggregated request
		stats_aggregation(req->globalinfo);
		req->reqnb = 1; //we need to set it like this otherwise the request_cleanup function will try to free the sub-requests, but they were inserted in the dispatch queue and we will only free them after the release
		request_cleanup(req);
	}
}
/**
 * This function is called by the release function, when the library user signaled it finished processing a request. In the case of a virtual request, its requests will be signaled separately, so here we are sure to receive a single request.
 * @param req the request that has been released by the user.
 */
void generic_cleanup(struct request_t *req)
{
	//update the processed requests counter
	req->globalinfo->stats.processedreq_nb++;
	//update the data counter
	req->globalinfo->stats.processed_req_size += req->io_data.len;
	request_cleanup(req); //remove from the list and free the memory
}
