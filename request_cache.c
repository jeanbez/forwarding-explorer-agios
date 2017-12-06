/* File:	request_cache.c
 * Created: 	2012
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It handles the data structures that keep requests
 *		Further information is available at http://inf.ufrgs.br/~fzboito/agios.html
 *
 * Contributors:
 *		Federal University of Rio Grande do Sul (UFRGS)
 *		INRIA France
 *
 *		inspired in Adrien Lebre's aIOLi framework implementation
 *
 *		This program is distributed in the hope that it will be useful,
 * 		but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */


#ifndef AGIOS_KERNEL_MODULE
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#else
#include <linux/mutex.h>
#include <linux/slab.h>
#endif  //ifndef AGIOS_KERNEL_MODULE - else

#include "agios.h"
#include "mylist.h"
#include "hash.h"
#include "proc.h"
#include "request_cache.h"
#include "predict.h"
#include "trace.h"
#include "agios_config.h"

#include "req_hashtable.h"
#include "req_timeline.h"
#include "consumer.h"

#include "common_functions.h"
#include "NOOP.h"
#include "performance.h"
#include "agios_request.h"

#include "pattern_tracker.h"

#ifdef AGIOS_KERNEL_MODULE
/*
 * Slab cache
 * It is used to manage frequently alocated structures:
 * request_t and request_file_t
 */
struct kmem_cache *request_cachep;
struct kmem_cache *request_file_cachep;
#endif

/**********************************************************************************************************************/
/*	COUNTERS	*/
/**********************************************************************************************************************/
int current_reqnb; //request counter
int current_reqfilenb; //files being accessed counter
pthread_mutex_t current_reqnb_lock = PTHREAD_MUTEX_INITIALIZER; //lock to protect these counters

//we increase this number at every new request, just so each one of them has an unique identifier
static int last_timestamp=0;

int get_current_reqnb()
{
	int ret;
	pthread_mutex_lock(&current_reqnb_lock);
	ret = current_reqnb;
	pthread_mutex_unlock(&current_reqnb_lock);
	return ret;
}
void inc_current_reqnb()
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_reqnb++;
	pthread_mutex_unlock(&current_reqnb_lock);
}
/*must hold mutex to the hashtable line*/
void dec_current_reqnb(int hash)
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_reqnb--;
	hashlist_reqcounter[hash]--;
	pthread_mutex_unlock(&current_reqnb_lock);
}
/*must hold mutex to the hashtable line*/
void dec_many_current_reqnb(int hash, int value)
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_reqnb-= value;
	hashlist_reqcounter[hash]-= value;
	pthread_mutex_unlock(&current_reqnb_lock);
}
void inc_current_reqfilenb()
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_reqfilenb++;
	pthread_mutex_unlock(&current_reqnb_lock);
}
void dec_current_reqfilenb()
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_reqfilenb--;
	pthread_mutex_unlock(&current_reqnb_lock);
}


void print_request(struct request_t *req)
{
	struct request_t *aux_req;

	if(req->reqnb > 1)
	{
		debug("\t\t\t%lu %lu", req->io_data.offset, req->io_data.len);
		debug("\t\t\t\t\t(virtual request size %d)", req->reqnb);
		agios_list_for_each_entry(aux_req, &req->reqs_list, related)
			debug("\t\t\t\t\t(%lu %lu %s)", aux_req->io_data.offset, aux_req->io_data.len, aux_req->file_id);

	}
	else
		debug("\t\t\t%lu %lu", req->io_data.offset, req->io_data.len);
}

void print_hashtable_line(int i)
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
	int i;

	debug("Current hashtable status:");
	for(i=0; i< AGIOS_HASH_ENTRIES; i++) //go through the whole hashtable, one position at a time
	{
		print_hashtable_line(i);
	}
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
	int i;
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


/*
 * Frees @req
 */
void request_cache_free(struct request_t *req)
{
	if(req)
	{
		VERIFY_REQUEST(req);
		agios_free(req);
	}
}


/*
 * Function allocates new struct request_t and sets it's fields
 * to values passed as arguments.
 */
#ifdef ORANGEFS_AGIOS
struct request_t * request_constructor(char *file_id, short int type, long int offset, long int len, int64_t data,  long int arrival_time, short int state, int app_id)
#else
struct request_t * request_constructor(char *file_id, short int type, long int offset, long int len, void * data, long int arrival_time, short int state, int app_id)
#endif

{
	struct request_t *new;

	new = agios_alloc(sizeof(struct request_t));
	if (!new)
		return new;

	new->file_id = agios_alloc(sizeof(char)*(strlen(file_id)+2));
	if(!new->file_id)
	{
		free(new);
		return NULL;
	}
	strcpy(new->file_id, file_id);

	new->tw_app_id = app_id;
	new->type = type;
	new->data = data;
	new->io_data.offset = offset;
	new->io_data.len = len;
	new->sched_factor = 0;
	new->state = state;
	new->jiffies_64 = arrival_time;
	new->reqnb = 1;
	init_agios_list_head(&new->reqs_list);
	new->agg_head=NULL;
	last_timestamp++;
	new->timestamp = last_timestamp;
	init_agios_list_head(&new->related);

	new->mirror = NULL ;
	new->already_waited=0;


#ifdef AGIOS_DEBUG
	new->sanity = 123456;
#endif
	return new;
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
	int i;

	timeline_lock();
	for(i=0; i< AGIOS_HASH_ENTRIES; i++)
		hashtable_lock(i);
	PRINT_FUNCTION_EXIT;
}
void unlock_all_data_structures()
{
	PRINT_FUNCTION_NAME;

	int i;
	for(i=0; i<AGIOS_HASH_ENTRIES; i++)
		hashtable_unlock(i);
	timeline_unlock();
	PRINT_FUNCTION_EXIT;
}

/*
 * This function allocates memory and initializes related locks
 */
//returns 0 on success
int request_cache_init(int max_app_id)
{
	int ret=0;

	reset_global_reqstats(); //put all statistics to zero

	timeline_init(max_app_id); //initializes the timeline

#ifdef AGIOS_KERNEL_MODULE
	/*allocates slab caches of the most used types (request_t and request_file_t)
	 * so memory obtention will be faster during execution*/
	request_cachep = kmem_cache_create("agios_request", sizeof(struct request_t), 0, 0 /* maybe SLAB_HWCACHE_ALIGN? */, NULL);
	if (!request_cachep) {
		printk(KERN_ERR "AGIOS: cannot create request SLAB cache\n");
		return -ENOMEM;
	}
	request_file_cachep = kmem_cache_create("agios_request_file", sizeof(struct request_file_t), 0, 0, NULL);
	if (!request_file_cachep) {
		printk(KERN_ERR "AGIOS: cannot create request_file SLAB cache\n");
		return -ENOMEM;
	}
#endif
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
	//TODO we were having serious issues with this portion of code, go through it looking for the errors
/*	PRINT_FUNCTION_NAME;
	lock_all_data_structures();
	print_hashtable();
	hashtable_cleanup();
	print_timeline();
	timeline_cleanup();
	PRINT_FUNCTION_EXIT;*/
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
int insert_aggregations(struct request_t *req, struct agios_list_head *insertion_place, struct agios_list_head *list_head)
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
struct request_file_t *find_req_file(struct agios_list_head *hash_list, char *file_id, int state)
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

#ifdef ORANGEFS_AGIOS
int agios_add_request(char *file_id, short int type, long int offset, long int len, int64_t data, struct client *clnt, int app_id)
#else
int agios_add_request(char *file_id, short int type, long int offset, long int len, void * data, struct client *clnt, int app_id)
#endif
{
	struct request_t *req;
	struct timespec arrival_time;
	long hash;
	short int previous_needs_hashtable;
	long int timestamp;

	PRINT_FUNCTION_NAME;

	//build request_t structure and fill it for the new request
	agios_gettime(&(arrival_time));
	timestamp = get_timespec2llu(arrival_time);
	add_request_to_pattern(timestamp, offset, len, type, file_id);
	req = request_constructor(file_id, type, offset, len, data, timestamp, RS_HASHTABLE, app_id);

	if(!req)
		return -ENOMEM;

	//first acquire lock
	hash = AGIOS_HASH_STR(req->file_id) % AGIOS_HASH_ENTRIES;
	while(1)
	{
		previous_needs_hashtable = current_scheduler->needs_hashtable;
		if(previous_needs_hashtable)
			hashtable_lock(hash);
		else
			timeline_lock();
		if(previous_needs_hashtable != current_scheduler->needs_hashtable) //acquiring the lock means a memory wall, so we are sure to get the latest version of current_scheduler
		{
			//the other thread has migrated scheduling algorithms (and data structure) while we were waiting for the lock (so the lock is no longer the adequate one)
			if(previous_needs_hashtable)
				hashtable_unlock(hash);
			else
				timeline_unlock();
		}
		else
			break;
	}

	//add request to the right data structure
	if(current_scheduler->needs_hashtable)
		hashtable_add_req(req,hash,NULL);
	else
		timeline_add_req(req, hash, NULL);

	//if(config_predict_agios_request_aggregation)
	//	prediction_newreq(req);
//TODO predict


	hashlist_reqcounter[hash]++;
	req->globalinfo->current_size += req->io_data.len;
	req->globalinfo->req_file->timeline_reqnb++;

	proc_stats_newreq(req);  //update statistics
	if(config_trace_agios)
		agios_trace_add_request(req);  //trace this request arrival
	inc_current_reqnb(); //increase the number of current requests on the scheduler

	/* Signalize to the consumer thread that new request was added. */
	if(current_alg != NOOP_SCHEDULER)
		consumer_signal_new_reqs();

	debug("current status: there are %d requests in the scheduler to %d files",current_reqnb, current_reqfilenb);

	//if we are running the NOOP scheduler, we just process it already
	if(current_alg == NOOP_SCHEDULER)
	{
		short int update_time;

		debug("NOOP is directly processing this request");
		update_time = process_requests(req, clnt, hash);
		generic_post_process(req);
		if(update_time)
			consumer_signal_new_reqs(); //if it is time to refresh something (change the scheduling algorithm or recalculate alpha), we wake up the scheduling thread
	}

	//free the lock
	if(previous_needs_hashtable)
		hashtable_unlock(hash);
	else
	{
		timeline_unlock();
	}
	PRINT_FUNCTION_EXIT;
	return 0;
}

//when agios is used to schedule requests to a parallel file system server, the stripe size is relevant to some calculations (specially for algorithm selection). A default value is provided through the configuration file, but many file systems (like PVFS) allow for each file to have different configurations. In this situation, the user could call this function to update a specific file's stripe size
int agios_set_stripe_size(char *file_id, int stripe_size)
{
	struct request_file_t *req_file;
	long hash_val = AGIOS_HASH_STR(file_id) % AGIOS_HASH_ENTRIES;
	short int previous_needs_hashtable;

	//find the structure for this file (and acquire lock)
	while(1)
	{
		previous_needs_hashtable = current_scheduler->needs_hashtable;
		if(previous_needs_hashtable)
			hashtable_lock(hash_val);
		else
			timeline_lock();
		if(previous_needs_hashtable != current_scheduler->needs_hashtable)
		{
			if(previous_needs_hashtable)
				hashtable_unlock(hash_val);
			else
				timeline_unlock();
		}
		else
			break;
	}

	req_file = find_req_file(&hashlist[hash_val], file_id, RS_HASHTABLE);
	req_file->stripe_size = stripe_size;

	if(previous_needs_hashtable)
		hashtable_unlock(hash_val);
	else
		timeline_unlock();
	return 1;
}
