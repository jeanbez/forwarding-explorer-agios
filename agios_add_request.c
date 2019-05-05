/*! \file agios_add_request.c
    \brief Implementation of the agios_add_request function, used by the user to add requests to AGIOS.

    TODO details
*/  
#include <stdbool.h>
 
#include "agios.h"
#include "agios_request.h"
#include "pattern_tracker.h"

/**
 * says if two requests to the same file are contiguous or not.
 */
#define CHECK_AGGREGATE(req,nextreq) \
     ( (req->offset <= nextreq->offset)&& \
         ((req->offset+req->len)>=nextreq->offset))

static int32_t g_last_timestamp=0; /**< We increase this number at every new request, just so each one of them has an unique identifier. */

/**
 * initializes the related_list_statistics_t structure from a queue.
 * @param stats the structured to be initialized.
 */
void request_file_init_related_statistics(struct related_list_statistics_t *stats)
{
	stats->processedreq_nb=0;
	stats->receivedreq_nb=0;
	stats->processed_req_size=0;
	stats->processed_bandwidth=-1;
	stats->releasedreq_nb=0;
	stats->avg_req_size = -1;
	stats->avg_time_between_requests = -1;
	stats->avg_distance = -1;
	stats->aggs_no = 0;
	stats->sum_of_agg_reqs = 0;
}
/** 
 * initializes a queue (struct related_list_t).
 * @param related_list the queue.
 * @param the file for which this queue is.
 */
void request_file_init_related_list(struct related_list_t *related_list, 
					struct request_file_t *req_file)
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
	related_list->shift_phenomena = 0;
	related_list->better_aggregation = 0;
	related_list->predicted_better_aggregation = 0;
	request_file_init_related_statistics(&related_list->stats);
}
/** 
 * Initializes a request_file_t structure about a file.
 * @param req_file the structure to be initialized.
 * @param file_id the file handle.
 * @return true or false for success
 */
bool request_file_init(struct request_file_t *req_file, 
			char *file_id)
{
	req_file->file_id = malloc(sizeof(char)*(strlen(file_id)+2));
	if (!req_file->file_id) return false;
	strcpy(req_file->file_id, file_id);
	req_file->first_request_time=0;
	req_file->first_request_predicted_time=-1;
	req_file->waiting_time = 0;
	req_file->wrote_simplified_trace=0;
	req_file->timeline_reqnb=0;
	request_file_init_related_list(&req_file->related_reads, req_file);
	request_file_init_related_list(&req_file->related_writes, req_file);
	return true;
}
/**
 * Create a virtual request from a "single request". For that, we need to create a new request_t structure to keep the virtual request, add it to the queue in place of aggregation_head, and include aggregation_head in its internal list.
 * @param aggregation_head is a normal request which is about to become a virtual request upon aggregation with another contiguous request. 
 * @param prev and next give the position of the request in its queue.
 * @return a pointer to the new virtual request.
 */
struct request_t *make_virtual_request(struct request_t *aggregation_head, 
					struct agios_list_head *prev, 
					struct agios_list_head *next)
{
	struct request_t *newreq; /**< the aggregated request we will create and fill and add to the hashtable. */

	/*creates a new request to be the aggregation head by copying all its information*/
	newreq = request_constructor(aggregation_head->file_id, 
					aggregation_head->type, 
					aggregation_head->offset, 
					aggregation_head->len, 
					0, 
					aggregation_head->arrival_time, 
					aggregation_head->queue_id);
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
/**
 * Aggregates two requests by inserting a single request in a virtual one. 
 * @param req the request to be included, it is not in the queue yet. 
 * @param agg_req the virtual request that is already in the queue. It may be updated by this function in case it is not really a virtual request yet.
 */
void include_in_aggregation(struct request_t *req, struct request_t **agg_req)
{
	struct agios_list_head *prev; /**< prev and next define the position of the virtual request in the queue. */
	struct agios_list_head *next;

	if ((*agg_req)->reqnb == 1) { /*agg_req is not a virtual request yet, we have to prepare it*/
		prev = (*agg_req)->related.prev;
		next = (*agg_req)->related.next;
		agios_list_del(&((*agg_req)->related));
		(*agg_req) = make_virtual_request((*agg_req), prev, next);
	}
	if (req->offset <= (*agg_req)->offset) { /*it has to be inserted in the beginning*/
		agios_list_add(&req->related, &((*agg_req)->reqs_list));
		(*agg_req)->len += (*agg_req)->offset - req->offset;
		(*agg_req)->offset = req->offset;
	}
	else { /*it has to be inserted in the end*/
		agios_list_add_tail(&req->related, &((*agg_req)->reqs_list));
		(*agg_req)->len = (*agg_req)->len + ((req->offset + req->len) - ((*agg_req)->offset + (*agg_req)->len));
	}
	(*agg_req)->reqnb++;
	if((*agg_req)->arrival_time > req->arrival_time)
		(*agg_req)->arrival_time = req->arrival_time;
	if((*agg_req)->timestamp > req->timestamp)
		(*agg_req)->timestamp = req->timestamp;
	(*agg_req)->sched_factor += req->sched_factor;
	req->agg_head = (*agg_req);
}
/**
 * function called when we have two virtual requests which are going to become one because we've added a new one which fills the gap between them. Aggregate them into a single virtual request.
 * @param head and tail the virtual requests (which will be updated in this function).
 */
void join_aggregations(struct request_t **head, struct request_t **tail)
{
	struct request_t *req; /**< used to iterate over all sub-requests from tail. */
	struct request_t *aux_req=NULL; /**< used to avoid moving a request before moving the iterator to the next one, otherwise the loop breaks. */

	/*removes the tail from the queue*/
	agios_list_del(&((*tail)->related));
	if ((*tail)->reqnb == 1) include_in_aggregation(*tail, head); /*the tail is not actually a virtual request*/
	else { /*the tail is a virtual request*/
		/*transfers all requests from this virtual request to the first one*/
		agios_list_for_each_entry (req, &(*tail)->reqs_list, related) {
			if (aux_req) {
				agios_list_del(&aux_req->related);
				include_in_aggregation(aux_req, head);
			}
			aux_req = req;
		}
		if (aux_req) {
			agios_list_del(&aux_req->related);
			include_in_aggregation(aux_req, head);
		}
		agios_free(*tail);	/*we dont need this empty virtual request anymore*/
		*tail = NULL;
	} //end tail is a virtual request 
}
/**
 * upon the insertion of a new request, checks if it is possible to include it into an existing virtual request (if yes, perform the aggregations already).
 * @param req the new request.
 * @param insertion_place the position of the queue where it is going to be inserted
 * @param the head of the queue where it is going to be inserted.
 * @return true or false to tell if the request was added into an aggregation or not. If it wasn't, it still has to be inserted in the queue after this call.
 */
int32_t insert_aggregations(struct request_t *req, 
				struct agios_list_head *insertion_place, 
				struct agios_list_head *list_head)
{
	struct request_t *prev_req; /**< the request immediately before the new one. */
	struct request_t *next_req; /**< the request immediately after the new one. */
	bool aggregated = false; /**< if we were able to aggregate the new request or not. */

	if (agios_list_empty(list_head)) return false; //check if the list is empty
	if (insertion_place != list_head) {
		/*if it is not the first request of the queue, we could aggregate it with the previous one*/
		prev_req = agios_list_entry(insertion_place, struct request_t, related);
		if (CHECK_AGGREGATE(prev_req, req) && ((prev_req->reqnb + req->reqnb) <= current_scheduler->max_aggreg_size)) { //if we should aggregate these requests
			if (req->reqnb > 1) join_aggregations(&prev_req, &req);
			else include_in_aggregation(req,&prev_req);
			insertion_place = &(prev_req->related);
			aggregated=true;
			/*maybe this request is also contiguous to the next one, so we will join everything*/
			if (insertion_place->next != list_head) { /*if the request was not to be the last of the queue*/
				next_req = agios_list_entry(insertion_place->next, struct request_t, related);
				if (CHECK_AGGREGATE(prev_req, next_req) && ((next_req->reqnb + prev_req->reqnb) <= current_scheduler->max_aggreg_size)) join_aggregations(&prev_req, &next_req); //if we should aggregate
			}
		} //end if we should aggregate
	} //end check aggregation with the previous one
	if ((!aggregated) && (insertion_place->next != list_head)) {
		/*if we could not aggregated with the previous one, or there is no previous one, and this request is not to be the last of the queue, lets try with the next one*/
		next_req = agios_list_entry(insertion_place->next, struct request_t, related);
		if (CHECK_AGGREGATE(req, next_req) && ((next_req->reqnb + req->reqnb) <= current_scheduler->max_aggreg_size)) { //if we should aggregate
			if (req->reqnb > 1) join_aggregations(&req, &next_req); //we could be adding a virtual request (because we are migrating between data structures), and then if we get here we will not add this new request anywhere, we'll actually remove the next one and copy its requests to the new one's list. So we cannot return aggregated = 1, because we still need to add this request
			else {
				include_in_aggregation(req, &next_req);
				aggregated=true;
			}
		} //end if we should aggregate
	} //end check aggregation with the next one
	return aggregated;
}
/** 
 * goes through a list of request_file_t structures searching for the given file_id. If such structure does not exist in the list, creates a new one and includes it. The caller MUST hold relevant lock (timeline or hashtable entry).
 * @param hash_list the list of request_file_t structures where we will look.
 * @param file_id the file handle.
 * @return a pointer to the found or newly allocated struct request_file_t of file_id. NULL in case of error.
 */
struct request_file_t *find_req_file(struct agios_list_head *hash_list, 
					char *file_id)
{
	struct request_file_t *req_file; /**< pointer that will be returned with the relevant file information. */
	bool found_reqfile= false; /**< as the name suggests. */
	bool found_higher_handle = false; /**< in case we stop iterating over the list because files have higher handles (since the list is ordered by file handle) */

	agios_list_for_each_entry (req_file, hash_list, hashlist) { //look for it in the list
		if (strcmp(req_file->file_id, file_id) >= 0) {
			if (strcmp(req_file->file_id, file_id) == 0) found_reqfile=true;
			else found_higher_handle = true;
			break; //since the list is ordered by file handle, we don't need to go through all files
		}
	} //end for all files in the list
	if (!found_reqfile) { //if we did not find it, make a new one
		struct agios_list_head *insertion_place; /**< used to know where to insert the newly allocated request_file_t. */
		if (found_higher_handle) insertion_place = &(req_file->hashlist);
		else insertion_place = hash_list;
		req_file = request_file_constructor(file_id);
		if (!req_file) {
			agios_print("PANIC! AGIOS could not allocate memory!\n");
			return NULL;
		}
		agios_list_add_tail(&req_file->hashlist, insertion_place);
	} //end if we did not find the structure
	//update the file counter (that keeps track of how many files are being accessed right now
	if (req_file->timeline_reqnb == 0) inc_current_reqfilenb();
	return req_file;
}
/**
 * Allocates and initializes a new request_file_t structure about a file.
 * @param file_id the file handle.
 * @return the newly allocated and initialized structure, or NULL in case of error.
 */
struct request_file_t * request_file_constructor(char *file_id)
{
	struct request_file_t *req_file;

	req_file = malloc(sizeof(struct request_file_t));
	if (!req_file) return NULL;
	if (!request_file_init(req_file, file_id)) { //we enter the if if we had problems filling the structure, in that case cleanup
		free(req_file);
		return NULL;
	}
	return req_file;
}
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
