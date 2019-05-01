/*! \file data_structures.c
    \brief TODO

    @see agios_request.h
    @see req_hashtable.c
    @see req_timeline.c
    @see agios_add_request.c 
    Depending on the scheduling algorithm being used, requests will be organized in different data structures. For instance, aIOLi and SJF use a hashtable, TO and TO-agg use a timeline, and TWINS uses multiple timelines. No matter the data structure used to hold the requests, AGIOS will always maintain the hashtable, because it is used for the statistics. 
*/

#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>
#include <limits.h>


//for debug 
void print_request(struct request_t *req)
{
	struct request_t *aux_req;

	if(req->reqnb > 1)
	{
		debug("\t\t\t%ld %ld", req->io_data.offset, req->io_data.len);
		debug("\t\t\t\t\t(virtual request size %d)", req->reqnb);
		agios_list_for_each_entry(aux_req, &req->reqs_list, related)
			debug("\t\t\t\t\t(%ld %ld %s)", aux_req->io_data.offset, aux_req->io_data.len, aux_req->file_id);

	}
	else
		debug("\t\t\t%ld %ld", req->io_data.offset, req->io_data.len);
}
//for debug
void print_hashtable_line(int32_t i)
{
	struct agios_list_head *hash_list;
	struct request_file_t *req_file;
	struct request_t *req;

	hash_list = &hashlist[i];
	if(!agios_list_empty(hash_list))
		debug("[%d]", i);
	agios_list_for_each_entry(req_file, hash_list, hashlist)
	{
		debug("\t%s", req_file->file_id);
		if(!(agios_list_empty(&req_file->related_reads.list) && agios_list_empty(&req_file->related_reads.dispatch)))
		{
			debug("\t\tread");
			agios_list_for_each_entry(req, &req_file->related_reads.list, related)
				print_request(req);
			debug("\t\tdispatch read");
			agios_list_for_each_entry(req, &req_file->related_reads.dispatch, related)
				print_request(req);
		}
		if(!(agios_list_empty(&req_file->related_writes.list) && agios_list_empty(&req_file->related_writes.dispatch)))
		{
			debug("\t\twrite");
			agios_list_for_each_entry(req, &req_file->related_writes.list, related)
				print_request(req);
			debug("\t\tdispatch writes");
			agios_list_for_each_entry(req, &req_file->related_writes.dispatch, related)
				print_request(req);

		}
	}

}

//debug functions, clean after
void print_hashtable(void)
{
	int32_t i;

	debug("Current hashtable status:");
	for(i=0; i< AGIOS_HASH_ENTRIES; i++) //go through the whole hashtable, one position at a time
	{
		print_hashtable_line(i);
	}
	PRINT_FUNCTION_EXIT;
}
void print_timeline(void)
{
	struct request_t *req;

	debug("Current timeline status:");
	debug("Requests:");
	agios_list_for_each_entry(req, &timeline, related)
	{
		print_request(req);
	}
}

/**********************************************************************************************************************/
/*	FUNCTIONS TO CHANGE THE CURRENT DATA STRUCTURE BETWEEN HASHTABLE AND TIMELINE	*/
/**********************************************************************************************************************/
void put_all_requests_in_timeline(struct agios_list_head *related_list, struct request_file_t *req_file, long hash);
void put_this_request_in_timeline(struct request_t *req, long hash, struct request_file_t *req_file)
{
	//remove from queue
	agios_list_del(&req->related);
	if((req->reqnb > 1) && (current_scheduler->max_aggreg_size <= 1))
	{
		//this is a virtual request, we need to break it into parts
		put_all_requests_in_timeline(&req->reqs_list, req_file, hash);
		if(req->file_id)
			agios_free(req->file_id);
		agios_free(req);
	}
	else
		//put in timeline
		timeline_add_req(req, hash, req_file);


}
//take all requests from a list and place them in the timeline
void put_all_requests_in_timeline(struct agios_list_head *related_list, struct request_file_t *req_file, long hash)
{
	struct request_t *req, *aux_req=NULL;

	agios_list_for_each_entry(req, related_list, related)
	{
		if(aux_req)
			put_this_request_in_timeline(aux_req, hash, req_file);
		aux_req = req;
	}
	if(aux_req)
		put_this_request_in_timeline(aux_req, hash, req_file);
}
void put_all_requests_in_hashtable(struct agios_list_head *list);
void put_req_in_hashtable(struct request_t *req)
{
	long hash;

	hash = AGIOS_HASH_STR(req->file_id) % AGIOS_HASH_ENTRIES;
	agios_list_del(&req->related);
	if((req->reqnb > 1) && (current_scheduler->max_aggreg_size <= 1))
	{
		put_all_requests_in_hashtable(&req->reqs_list);
		if(req->file_id)
			agios_free(req->file_id);
		agios_free(req);
	}
	else
		hashtable_add_req(req, hash, req->globalinfo->req_file);
}
void put_all_requests_in_hashtable(struct agios_list_head *list)
{
	struct request_t *req, *aux_req=NULL;

	agios_list_for_each_entry(req, list, related)
	{
		if(aux_req)
			put_req_in_hashtable(aux_req);
		aux_req = req;
	}
	if(aux_req)
		put_req_in_hashtable(aux_req);
}
//gets all requests from hashtable and move them to the timeline.
//No need to hold mutexes, but NO OTHER THREAD may be using any of these data structures.
void migrate_from_hashtable_to_timeline()
{
	int32_t i;
	struct agios_list_head *hash_list;
	struct request_file_t *req_file;

	//we will mess with the data structures and don't even use locks, since here we are certain no one else is messing with them
	for(i=0; i< AGIOS_HASH_ENTRIES; i++) //go through the whole hashtable, one position at a time
	{
		hash_list = &hashlist[i];
		agios_list_for_each_entry(req_file, hash_list, hashlist)
		{
			//get all requests from it and put them in the timeline
			put_all_requests_in_timeline(&req_file->related_writes.list, req_file, i);
			put_all_requests_in_timeline(&req_file->related_reads.list, req_file, i);
		}
	}
}
//gets al requests from timeline and move them to the hashtable. Also move all request_file_t structures to the hashtable to we keep statistics. No need to hold mutexes, but NO OTHER THREAD may be using any of these data structures
void migrate_from_timeline_to_hashtable()
{
	put_all_requests_in_hashtable(&timeline);
}



//to each file, we will have related_list structures for read and write queues (and sometimes for predicted queues as well)
//even when requests are kept in timeline (not in these queues), we still have the structures to keep track of statistics
void request_file_init_related_statistics(struct related_list_statistics_t *stats)
{
	stats->processedreq_nb=0;
	stats->receivedreq_nb=0;

	stats->processed_req_size=0;
	stats->processed_req_time=0;

	stats->total_req_size = 0;
	stats->min_req_size = LONG_MAX;
	stats->max_req_size=0;

	stats->max_request_time = 0;
	stats->total_request_time = 0;
	stats->min_request_time = LONG_MAX;

	stats->avg_distance = -1;
	stats->avg_distance_count = 1;

	stats->aggs_no = 0;
	stats->sum_of_agg_reqs = 0;
}
//this function initializes a related_list structure
void request_file_init_related_list(struct related_list_t *related_list, struct request_file_t *req_file)
{
	init_agios_list_head(&related_list->list);
	init_agios_list_head(&related_list->dispatch);
	related_list->req_file = req_file;

	related_list->laststartoff = 0;
	related_list->lastfinaloff = 0;
	related_list->predictedoff = 0;

	related_list->nextquantum = 0;

	related_list->current_size = 0;

	related_list->lastaggregation = 0;
	related_list->best_agg = 0;

	related_list->last_received_finaloffset = 0;

#ifndef ORANGEFS_AGIOS
	related_list->avg_stripe_difference= -1;
#endif

	related_list->spatiality = -1;
	related_list->app_request_size = -1;

	related_list->shift_phenomena = 0;
	related_list->better_aggregation = 0;
	related_list->predicted_better_aggregation = 0;

	request_file_init_related_statistics(&related_list->stats_file);
	request_file_init_related_statistics(&related_list->stats_window);
}
/*
 * Function sets fields of struct request_file_t @req_file to default values.
 */
static void request_file_init(struct request_file_t *req_file, char *file_id)
{
	req_file->file_id = agios_alloc(sizeof(char)*(strlen(file_id)+2));
	strcpy(req_file->file_id, file_id);

	req_file->first_request_time=0;
	req_file->first_request_predicted_time=-1;
	req_file->waiting_time = 0;
	req_file->wrote_simplified_trace=0;
	req_file->timeline_reqnb=0;

	req_file->stripe_size = config_agios_stripe_size;

	request_file_init_related_list(&req_file->related_reads, req_file);
	request_file_init_related_list(&req_file->related_writes, req_file);
	request_file_init_related_list(&req_file->predicted_reads, req_file);
	request_file_init_related_list(&req_file->predicted_writes, req_file);
}

/*
 * Function allocates new struct request_file_t
 * and initializes it's fields to default values.
 *
 * Arguments:
 *	@file_id	- id of the file we're allocating structure for
 *
 * Returns:
 *	struct request_file * or NULL
 */
struct request_file_t * request_file_constructor(char *file_id)
{
	struct request_file_t *req_file;

	req_file = agios_alloc(sizeof(struct request_file_t));
	if (!req_file)
		return req_file;

	request_file_init(req_file, file_id);
	return req_file;
}

/*
 * locks (and unlocks) all data structures used for requests and files
 * It is not supposed to be used for normal library functions.
 * We only use them at initialization, to guarantee the user won't try
 * to add new requests while we did not decide on the scheduling algorithm
 * yet. Moreover, we use these functions while migrating between scheduling
 * algorithms
 */
void lock_all_data_structures()
{
	PRINT_FUNCTION_NAME;
	int32_t i;

	timeline_lock();
	for(i=0; i< AGIOS_HASH_ENTRIES; i++)
		hashtable_lock(i);
	PRINT_FUNCTION_EXIT;
}
void unlock_all_data_structures()
{
	PRINT_FUNCTION_NAME;

	int32_t i;
	for(i=0; i<AGIOS_HASH_ENTRIES; i++)
		hashtable_unlock(i);
	timeline_unlock();
	PRINT_FUNCTION_EXIT;
}

/*
 * This function allocates memory and initializes related locks
 */
//returns 0 on success
int32_t request_cache_init(int32_t max_app_id)
{
	int32_t ret=0;

	reset_global_reqstats(); //puts all statistics to zero (from the proc module)

	if((ret = timeline_init(max_app_id)) != 0) //initializes the timeline
		return ret;

	ret = hashtable_init(); //initializes the hashtable

	//put request and file counters to 0
	if(ret == 0)
	{
		current_reqnb = 0;
		current_reqfilenb=0;
	}

	lock_all_data_structures();

	return ret;
}

//free the space used by a struct request_t, which describes a request
//if the request is aggregated (with multiple requests inside), it will recursively free these requests as well
void request_cleanup(struct request_t *aux_req)
{
	//remove the request from its queue
	agios_list_del(&aux_req->related);
	//see if it is a virtual request
	if(aux_req->reqnb > 1)
	{
		//free all sub-requests
		list_of_requests_cleanup(&aux_req->reqs_list);
	}
	//free the memory space
	if(aux_req->file_id)
		agios_free(aux_req->file_id);
	agios_free(aux_req);
}
//free all requests from a queue
void list_of_requests_cleanup(struct agios_list_head *list)
{
	struct request_t *req, *aux_req = NULL;

	if(!agios_list_empty(list))
	{
		agios_list_for_each_entry(req, list, related)
		{
			if(aux_req)
				request_cleanup(aux_req);
			aux_req = req;
		}
		if(aux_req)
			request_cleanup(aux_req);
	}
}
/*
 * Function destroys all SLAB caches previously created by request_cache_init().
 */
void request_cache_cleanup(void)
{
	hashtable_cleanup();
	timeline_cleanup();
}

//aggregation_head is a normal request which is about to become a virtual request upon aggregation with another contiguous request.
//for that, we need to create a new request_t structure to keep the virtual request, add it to the hashtable in place of aggregation_head, and include aggregation_head in its internal list
struct request_t *start_aggregation(struct request_t *aggregation_head, struct agios_list_head *prev, struct agios_list_head *next)
{
	struct request_t *newreq;

	/*creates a new request to be the aggregation head*/
	newreq = request_constructor(aggregation_head->file_id, aggregation_head->type, aggregation_head->io_data.offset, aggregation_head->io_data.len, 0, aggregation_head->jiffies_64, RS_HASHTABLE, aggregation_head->tw_app_id);
	newreq->sched_factor = aggregation_head->sched_factor;
	newreq->timestamp = aggregation_head->timestamp;

	/*replaces the request on the hashtable*/
	__agios_list_add(&newreq->related, prev, next);
	newreq->globalinfo = aggregation_head->globalinfo;
	aggregation_head->agg_head = newreq;

	/*adds the replaced request on the requests list of the aggregation head*/
	agios_list_add_tail(&aggregation_head->related, &newreq->reqs_list);

	return newreq;
}

/*includes req in the aggregation agg_req */
void include_in_aggregation(struct request_t *req, struct request_t **agg_req)
{
	struct agios_list_head *prev, *next;

	PRINT_FUNCTION_NAME;

	if((*agg_req)->reqnb == 1) /*this is not a virtual request yet, we have to prepare it*/
	{
		prev = (*agg_req)->related.prev;
		next = (*agg_req)->related.next;
		agios_list_del(&((*agg_req)->related));
		(*agg_req) = start_aggregation((*agg_req), prev, next);
	}
	if(req->io_data.offset <= (*agg_req)->io_data.offset) /*it has to be inserted in the beginning*/
	{
		agios_list_add(&req->related, &((*agg_req)->reqs_list));
		(*agg_req)->io_data.len += (*agg_req)->io_data.offset - req->io_data.offset;
		(*agg_req)->io_data.offset = req->io_data.offset;
	}
	else /*it has to be inserted in the end*/
	{
		agios_list_add_tail(&req->related, &((*agg_req)->reqs_list));
		(*agg_req)->io_data.len = (*agg_req)->io_data.len + ((req->io_data.offset + req->io_data.len) - ((*agg_req)->io_data.offset + (*agg_req)->io_data.len));
	}
	(*agg_req)->reqnb++;
	if((*agg_req)->jiffies_64 > req->jiffies_64)
		(*agg_req)->jiffies_64 = req->jiffies_64;
	if((*agg_req)->timestamp > req->timestamp)
		(*agg_req)->timestamp = req->timestamp;
	(*agg_req)->sched_factor += req->sched_factor;
	req->agg_head = (*agg_req);
}
//we have two virtual requests which are going to become one because we've added a new one which fills the gap between them
void join_aggregations(struct request_t **head, struct request_t **tail)
{
	struct request_t *req, *aux_req=NULL;

	/*removes the tail from the list*/
	agios_list_del(&((*tail)->related));

	if((*tail)->reqnb == 1) /*it is not a virtual request*/
		include_in_aggregation(*tail, head);
	else /*it is a virtual request*/
	{
		/*transfers all requests from this virtual request to the first one*/
		agios_list_for_each_entry(req, &(*tail)->reqs_list, related)
		{
			if(aux_req)
			{
				agios_list_del(&aux_req->related);
				include_in_aggregation(aux_req, head);
			}
			aux_req = req;
		}
		if(aux_req)
		{
			agios_list_del(&aux_req->related);
			include_in_aggregation(aux_req, head);
		}
		agios_free(*tail);	/*we dont need this anymore*/
	}
}

/*upon the insertion of a new request, checks if it is possible to include it into an existing virtual request (if yes, perform the aggregations already)*/
int32_t insert_aggregations(struct request_t *req, struct agios_list_head *insertion_place, struct agios_list_head *list_head)
{
	struct request_t *prev_req, *next_req;
	int aggregated = 0;

	if(agios_list_empty(list_head))
		return 0;

	/*if it is not the first request of the queue, we could aggregate it with the previous one*/
	if(insertion_place != list_head)
	{
		prev_req = agios_list_entry(insertion_place, struct request_t, related);
		if(CHECK_AGGREGATE(prev_req, req) && ((prev_req->reqnb + req->reqnb) <= current_scheduler->max_aggreg_size)) //TODO  also check if it is worth it considering the required times? does it make sense, since requests are already here anyway, and that's why we have max_aggregation_size anyway...
		{
			if(req->reqnb > 1)
				join_aggregations(&prev_req, &req);
			else
				include_in_aggregation(req,&prev_req);
			insertion_place = &(prev_req->related);
			aggregated=1;
			/*maybe this request is also contiguous to the next one, so we will join everything*/
			if(insertion_place->next != list_head) /*if the request was not to be the last of the queue*/
			{
				next_req = agios_list_entry(insertion_place->next, struct request_t, related);
				if(CHECK_AGGREGATE(prev_req, next_req) && ((next_req->reqnb + prev_req->reqnb) <= current_scheduler->max_aggreg_size))
				{
					join_aggregations(&prev_req, &next_req);
				}
			}
		}
	}
	if((!aggregated) && (insertion_place->next != list_head)) /*if we could not aggregated with the previous one, or there is no previous one, and this request is not to be the last of the queue, lets try with the next one*/
	{
		next_req = agios_list_entry(insertion_place->next, struct request_t, related);
		if(CHECK_AGGREGATE(req, next_req) && ((next_req->reqnb + req->reqnb) <= current_scheduler->max_aggreg_size))
		{
			if(req->reqnb > 1)
				join_aggregations(&req, &next_req); //we could be adding a virtual request (because we are migrating between data structures), and then if we get here we will not add this new request anywhere, we'll actually remove the next one and copy its requests to the new one's list. So we cannot return aggregated = 1, because we still need to add this request
			else
			{
				include_in_aggregation(req, &next_req);
				aggregated=1;
			}
		}
	}
	return aggregated;
}

/* goes through a list of request_file_t structures searching for the given file_id.
 * if such structure does not exist in the list, creates a new one and includes it.
 * must hold relevant lock (timeline or hashtable entry).
 */
struct request_file_t *find_req_file(struct agios_list_head *hash_list, char *file_id, int32_t state)
{
	struct request_file_t *req_file;
	int found_reqfile=0;
	struct agios_list_head *insertion_place;

	/*
	 * Iterate through sorted collison list for this slot searching for
	 * entry about the file that @req treats with.
	 */
	agios_list_for_each_entry(req_file, hash_list, hashlist) {
		if (strcmp(req_file->file_id, file_id) >= 0) {
			found_reqfile=1;
			break;
		}
	}

	/*
	 * If we don't have other request for this file, we need to allocate new
	 * structure request_file_t.
	 */
	if ((agios_list_empty(hash_list)) || (found_reqfile == 0) || ((found_reqfile == 1) && (strcmp(req_file->file_id, file_id) != 0)))
	{
		if(found_reqfile)
			insertion_place = &(req_file->hashlist);
		else
			insertion_place = hash_list;
		req_file = request_file_constructor(file_id);
		if(!req_file)
		{
			agios_print("PANIC! AGIOS could not allocate memory!\n");
			exit(-1);
		}

		agios_list_add_tail(&req_file->hashlist, insertion_place);
	}

	//update the file counter (that keeps track of how many files are being accessed right now
	if((state == RS_PREDICTED) && (agios_list_empty(&req_file->predicted_reads.list)) && (agios_list_empty(&req_file->predicted_writes.list))) //we are adding a predicted requests, and this file doesnt have any
		inc_current_predicted_reqfilenb();
	else if(state == RS_HASHTABLE) //real request
	{
		if(req_file->timeline_reqnb == 0)
			inc_current_reqfilenb();
	}

	return req_file;
}


