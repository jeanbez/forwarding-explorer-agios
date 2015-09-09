/* File:	request_cache.c
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides and handles the data structures to store files' and requests' 
 *		information.
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

#include "common_functions.h"

extern struct consumer_t consumer;

#ifdef AGIOS_KERNEL_MODULE
/*
 * Slab cache
 * It is used to manage frequently alocated structures:
 * request_t and request_file_t
 */
struct kmem_cache *request_cachep;
struct kmem_cache *request_file_cachep;
#endif

/*
 * Time line
 * It contains all requests, the same as hash table but sorted by arrival time.
 */
static AGIOS_LIST_HEAD(timeline);
static AGIOS_LIST_HEAD(timeline_files);
#ifdef AGIOS_KERNEL_MODULE
static DEFINE_MUTEX(timeline_mutex);
#else
static pthread_mutex_t timeline_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/*
 * Hash table
 * Beacause we need to quickly find a place to store request for particular
 * file and/or find related requests, we use hash table that uses
 * request_t.file_id as value for hash function.
 */
static struct agios_list_head *hashlist;
static int *hashlist_reqcounter = NULL;
#ifdef AGIOS_KERNEL_MODULE
static struct mutex *hashlist_locks;
#else
static pthread_mutex_t *hashlist_locks;
#endif

static int current_reqnb;
static int current_reqfilenb;
static pthread_mutex_t current_reqnb_lock = PTHREAD_MUTEX_INITIALIZER;

static unsigned long long int last_timestamp=0;

static int max_aggregation_size;
static short int scheduler_needs_hashtable=1;

inline void set_max_aggregation_size(int value)
{
	max_aggregation_size = value;
}
inline short int get_needs_hashtable()
{
	return scheduler_needs_hashtable;
}

inline void set_needs_hashtable(short int value)
{
	scheduler_needs_hashtable = value;
}

inline int get_current_reqnb()
{
	int ret;
	pthread_mutex_lock(&current_reqnb_lock);
	ret = current_reqnb;
	pthread_mutex_unlock(&current_reqnb_lock);
	return ret;
}
inline int get_current_reqfilenb()
{
	int ret;
	pthread_mutex_lock(&current_reqnb_lock);
	ret = current_reqfilenb;
	pthread_mutex_unlock(&current_reqnb_lock);
	return ret;
}
inline void set_current_reqnb(int value)
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_reqnb = value;
	pthread_mutex_unlock(&current_reqnb_lock);
}
inline void set_current_reqfilenb(int value)
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_reqfilenb = value;
	pthread_mutex_unlock(&current_reqnb_lock);
}

inline void inc_current_reqnb()
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_reqnb++;
	pthread_mutex_unlock(&current_reqnb_lock);
}
/*must hold mutex to the hashtable line*/
inline void dec_current_reqnb(int hash)
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_reqnb--;
	pthread_mutex_unlock(&current_reqnb_lock);
	if(scheduler_needs_hashtable)
		hashlist_reqcounter[hash]--;
}
/*must hold mutex to the hashtable line*/
inline void dec_many_current_reqnb(int hash, int value)
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_reqnb-= value;
	pthread_mutex_unlock(&current_reqnb_lock);
	if(scheduler_needs_hashtable)
		hashlist_reqcounter[hash]-=value;
}

inline void inc_current_reqfilenb()
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_reqfilenb++;
	pthread_mutex_unlock(&current_reqnb_lock);
}
inline void dec_current_reqfilenb()
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_reqfilenb--;
	pthread_mutex_unlock(&current_reqnb_lock);
}
/*must hold the hashtable line mutex*/
inline int get_hashlist_reqcounter(int hash)
{
	return hashlist_reqcounter[hash];
}

inline void include_in_aggregation(struct request_t *req, struct request_t **agg_req);
/*
 * Adds @req to the end of timeline list.
 *
 * Locking: 
 *	Must NOT have timeline_mutex
 */
void timeline_add_req(struct request_t *req)
{
	struct request_file_t *req_file;
	struct request_t *tmp;
	int aggregated=0;
	int tw_priority;

	PRINT_FUNCTION_NAME;
	VERIFY_REQUEST(req);
	
	agios_mutex_lock(&timeline_mutex);

	/*find the file and update its informations if needed*/
	req_file = find_req_file(&timeline_files, req->file_id, req->state); //its the same function that adds a file to the hashtable. What happens is that a hashtable entry is actually a list of files (ordered by file name), just like timeline_files
	if(req_file->first_request_time == 0)
		req_file->first_request_time = req->jiffies_64;
	if(req->type == RT_READ)
		req->globalinfo = &req_file->related_reads;
	else
		req->globalinfo = &req_file->related_writes;
	req_file->timeline_reqnb++;
	
	/*get timestamp*/
	if (agios_list_empty(&timeline)) {
		req->timestamp = 0;
	} else {
		req->timestamp = ((struct request_t *)agios_list_entry(timeline.prev,
							struct request_t,
							related))->timestamp + 1;
	}	

	if (get_selected_alg() == TIME_WINDOW_SCHEDULER) 
	{
		// Calculate the request priority
		tw_priority = req->timestamp / TIME_WINDOW_SIZE * 32768 + req->tw_app_id;

		// Find the position to insert the request
		agios_list_for_each_entry(tmp, &timeline, related)
		{
			if (tmp->tw_priority > tw_priority) {
				agios_list_add(&req->related, &tmp->related);
				
				return;
			}
		}

		// If it was not inserted, insert the request in the proper position
		agios_list_add_tail(&req->related, &timeline);

		return;
	} 

	/*try to aggregate it*/
	if(get_selected_alg() != SIMPLE_TIMEORDER_SCHEDULER && get_selected_alg() != TIME_WINDOW_SCHEDULER)
	{	
		agios_list_for_each_entry(tmp, &timeline, related)
		{
			if(tmp->globalinfo == req->globalinfo) //same type and to the same file
			{
				if(tmp->reqnb < max_aggregation_size) 
				{
					if(CHECK_AGGREGATE(req,tmp) || CHECK_AGGREGATE(tmp, req)) //contiguous
					{
						include_in_aggregation(req, &tmp);
						aggregated=1;
						break;	
					}
				} 
			}
		}  
	}

	if(!aggregated) //if not aggregated, add it
		agios_list_add_tail(&req->related, &timeline); //we use the related list structure so we can reuse several (like include_in_aggregation) functions from the hashtable implementation

}

/*
 * Returns the oldest request and REMOVES it from timeline list.
 *
 * Returns:
 *	oldest request or NULL
 *
 * Locking: 
 *	Must have timeline_mutex
 */
struct request_t *timeline_oldest_req(void)
{
	struct request_t *tmp;

	//PRINT_FUNCTION_NAME;
	
	if (agios_list_empty(&timeline)) {
		return 0;
	}

	tmp = agios_list_entry(timeline.next, struct request_t, related);
	agios_list_del(&tmp->related);
	tmp->globalinfo->req_file->timeline_reqnb--;

	return tmp;
}


/*
 * Function locks timeline.
 * Used by I/O scheduler before iterating the list.
 *
 * Locking:
 *	Must NOT be holding timeline_mutex.
 *	Must call timeline_unlock later.
 */ 
inline struct agios_list_head *timeline_lock(void)
{
	agios_mutex_lock(&timeline_mutex);
	return &timeline;
}

/*
 * Function unlocks timeline.
 * Used by I/O scheduler after iterating through the list.
 *
 * Locking:
 *	Must be holding timeline_mutex.
 */ 
inline void timeline_unlock(void)
{
	agios_mutex_unlock(&timeline_mutex);
}

//must have timeline_mutex
inline struct agios_list_head *get_timeline_files(void)
{
	return &timeline_files;
}

/*
 * Frees @req by returning it to slab cache.
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
struct request_t * request_constructor(char *file_id, int type, long long offset, long len, int64_t data, unsigned long long int arrival_time, int state)
#else
struct request_t * request_constructor(char *file_id, int type, long long offset, long len, int data, unsigned long long int arrival_time, int state) 
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

	new->mirror = NULL ;
	new->already_waited=0;


#ifdef AGIOS_DEBUG
	new->sanity = 123456;
#endif
	return new;
}

void request_file_init_related_list(struct related_list_t *related_list, struct request_file_t *req_file)
{
	init_agios_list_head(&related_list->list);

	related_list->laststartoff = 0;
	related_list->lastfinaloff = 0;
	related_list->used_quantum_rate=100;
	related_list->predictedoff = 0;
	related_list->lastaggregation = 0;
	related_list->proceedreq_nb = 0;
	related_list->nextquantum = 0;
	related_list->current_size = 0;
	related_list->req_file = req_file;
	related_list->avg_distance = -1;
	related_list->avg_distance_count = 1;
	related_list->avg_stripe_difference=-1;
	related_list->spatiality = -1;
	related_list->app_request_size = -1;

	related_list->stats.aggs_no = 0;
	related_list->stats.sum_of_agg_reqs = 0;
	related_list->stats.biggest = 0;
	related_list->stats.shift_phenomena = 0;
	related_list->stats.better_aggregation = 0;
	related_list->stats.predicted_better_aggregation = 0;
	related_list->stats.total_req_size = 0;
	related_list->stats.min_req_size = ~0;
	related_list->stats.max_req_size = 0;
	related_list->stats.max_request_time = 0;
	related_list->stats.total_request_time = 0;
	related_list->stats.min_request_time = ~0;
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

	request_file_init_related_list(&req_file->related_reads, req_file);
	request_file_init_related_list(&req_file->related_writes, req_file);
	request_file_init_related_list(&req_file->predicted_reads, req_file);
	request_file_init_related_list(&req_file->predicted_writes, req_file);
}

/*
 * Function allocates new struct request_file_t from slab cache
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
 * This function allocates memory and initializes related locks 
 */
int request_cache_init(void)
{
	int i;

	reset_reqstats();

	/*at this point we dont know if we will be using timeline or hashtable or both, so we will have to allocate both of them*/
	init_agios_list_head(&timeline);
	init_agios_list_head(&timeline_files);

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

	hashlist = (struct agios_list_head *) agios_alloc(sizeof(struct agios_list_head) * AGIOS_HASH_ENTRIES);
	if(!hashlist)
	{
		agios_print("AGIOS: cannot allocate memory for req cache\n");
		return -ENOMEM;
	}

	hashlist_locks = agios_alloc(SIZE_OF_AGIOS_MUTEX_T*(AGIOS_HASH_ENTRIES+1));
	if(!hashlist_locks)
	{
		agios_print("AGIOS: cannot allocate memory for req locks\n");
		agios_free(hashlist);
		return -ENOMEM;
	}

	hashlist_reqcounter = agios_alloc(sizeof(int)*(AGIOS_HASH_ENTRIES+1));
	if(!hashlist_reqcounter)
	{
		agios_print("AGIOS: cannot allocate memory for req counters\n");
		agios_free(hashlist);
		agios_free(hashlist_locks);
		return -ENOMEM;
	}

	
	for (i = 0; i < AGIOS_HASH_ENTRIES; i++) {
		init_agios_list_head(&hashlist[i]);
		agios_mutex_init(&(hashlist_locks[i]));
		hashlist_reqcounter[i]=0;
	}

	set_current_reqnb(0);
	set_current_reqfilenb(0);
	
	return 0;
}

/*
 * Function destroys all SLAB caches previously created by request_cache_init().
 */
void request_cache_cleanup(void)
{
	agios_free(hashlist);
	agios_free(hashlist_locks);
	agios_free(hashlist_reqcounter);
}

inline struct request_t *start_aggregation(struct request_t *aggregation_head, struct agios_list_head *prev, struct agios_list_head *next)
{
	struct request_t *newreq;	

	/*creates a new request to be the aggregation head*/
	newreq = request_constructor(aggregation_head->file_id, aggregation_head->type, aggregation_head->io_data.offset, aggregation_head->io_data.len, 0, aggregation_head->jiffies_64, RS_HASHTABLE);
	newreq->sched_factor = aggregation_head->sched_factor;
	newreq->timestamp = aggregation_head->timestamp;

	/*replaces the request on the hashtable*/
	__agios_list_add(&newreq->related, prev, next);
	newreq->globalinfo = aggregation_head->globalinfo;
	aggregation_head->agg_head = newreq;

	/*adds the replaced request on the requests list of the aggregation head*/
	agios_list_add_tail(&aggregation_head->aggregation_element, &newreq->reqs_list);

	return newreq;
}

/*includes req in the aggregation agg_req */
inline void include_in_aggregation(struct request_t *req, struct request_t **agg_req)
{
	struct agios_list_head *prev, *next;
	
	if((*agg_req)->reqnb == 1) /*this is not a virtual request yet, we have to prepare it*/
	{
		prev = (*agg_req)->related.prev;
		next = (*agg_req)->related.next;
		agios_list_del(&((*agg_req)->related));
		(*agg_req) = start_aggregation((*agg_req), prev, next);	
	}
	if(req->io_data.offset <= (*agg_req)->io_data.offset) /*it has to be inserted in the beginning*/	
	{
		agios_list_add(&req->aggregation_element, &((*agg_req)->reqs_list));
		(*agg_req)->io_data.len += (*agg_req)->io_data.offset - req->io_data.offset;
		(*agg_req)->io_data.offset = req->io_data.offset;
	}
	else /*it has to be inserted in the end*/
	{
		agios_list_add_tail(&req->aggregation_element, &((*agg_req)->reqs_list));	
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

inline void join_aggregations(struct request_t *head, struct request_t *tail)
{
	struct agios_list_head *aux;
	int i;
	struct request_t *aux_req;

	/*removes the tail from the list*/
	agios_list_del(&tail->related);

	if(tail->reqnb == 1) /*it is not a virtual request*/
		include_in_aggregation(tail, &head);
	else /*it is a virtual request*/
	{
		/*transfers all requests from this virtual request to the first one*/
		aux = tail->reqs_list.next;
		for(i=0; i<tail->reqnb; i++)
		{
			aux_req = agios_list_entry(aux, struct request_t, aggregation_element);
			aux = aux->next;
			
			include_in_aggregation(aux_req, &head);
		}	
		free(tail);	/*we dont need this anymore*/	
	}
}

/*upon the insertion of a new request, checks if it is possible to include it into an existing virtual request (if yes, perform the aggregations already)*/
inline int insert_aggregations(struct request_t *req, struct agios_list_head *insertion_place, struct agios_list_head *list_head)
{
	struct request_t *prev_req, *next_req;
	int aggregated = 0;
	
	if(agios_list_empty(list_head))
		return 0;
	
	/*if it is not the first request of the queue, we could aggregate it with the previous one*/
	if(insertion_place != list_head)
	{
		prev_req = agios_list_entry(insertion_place, struct request_t, related);
		if(CHECK_AGGREGATE(prev_req, req) && (prev_req->reqnb < max_aggregation_size)) //TODO  also check if it is worth it considering the required times? does it make sense, since requests are already here anyway, and that's why we have max_aggregation_size anyway...
		{
			include_in_aggregation(req,&prev_req);
			insertion_place = &(prev_req->related);
			aggregated=1;
			/*maybe this request is also contiguous to the next one, so we will join everything*/
			if(insertion_place->next != list_head) /*if the request was not to be the last of the queue*/
			{
				next_req = agios_list_entry(insertion_place->next, struct request_t, related);
				if(CHECK_AGGREGATE(req, next_req) && (next_req->reqnb + prev_req->reqnb <= max_aggregation_size)) 
				{
					join_aggregations(prev_req, next_req);
				}
			}
		}
	}
	if((!aggregated) && (insertion_place->next != list_head)) /*if we could not aggregated with the previous one, or there is no previous one, and this request is not to be the last of the queue, lets try with the next one*/
	{
		next_req = agios_list_entry(insertion_place->next, struct request_t, related);
		if(CHECK_AGGREGATE(req, next_req) && (next_req->reqnb < max_aggregation_size)) 
		{
			include_in_aggregation(req, &next_req);
			aggregated=1;
		}
	}
	return aggregated;
}

/* searchs the hash_list (hashtable entry, MUST HOLD LOCK, for the file which req is trying to access and returns
 * a pointer to the struct request_file_t of this file. If there is no entry to this file, create one
 */
inline struct request_file_t *find_req_file(struct agios_list_head *hash_list, char *file_id, int state)
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
		debug("including a new request_file_t structure for file %s\n", file_id);
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
		if((scheduler_needs_hashtable) && (agios_list_empty(&req_file->related_reads.list)) && (agios_list_empty(&req_file->related_writes.list))) //request is being included in the hashtable on req_file->related_reads or req_file->related_writes
			inc_current_reqfilenb();
		else if ((!scheduler_needs_hashtable) && (req_file->timeline_reqnb  == 0)) //request is being included in the timeline, req_file->timeline_reqnb keeps track of how many requests are for this file
			inc_current_reqfilenb();
	}

	return req_file;
}
/*
 * This is internal function which adds @req to hash table and places it
 * according to @hash_val.
 *
 * Locking:
 * 	Must be holding hashlist_locks[hash_val]
 */
static void __hashtable_add_req(struct request_t *req, unsigned long hash_val)
{
	struct agios_list_head *hash_list = &hashlist[hash_val];
	struct request_file_t *req_file;
	struct request_t *tmp;
	struct agios_list_head *insertion_place;


#ifdef AGIOS_DEBUG
	if(req->state == RS_HASHTABLE)
		debug("adding request [%d] to file %s, offset %lld, size %lld", req->data, req->file_id, req->io_data.offset, req->io_data.len);
#endif

	/*finds the file to add to*/
	req_file = find_req_file(hash_list, req->file_id, req->state);
	
	/*if it is the first request to this file (first actual request, anyway, it could have predicted requests only), we have to store the arrival time*/
	if(req->state != RS_PREDICTED)
	{
		/*see if this is the first request to the file*/
		if(req_file->first_request_time == 0)
			req_file->first_request_time = req->jiffies_64;
	}

	/*
	 * Now we have request_file_t structure describing our file,
	 * so add @req to one of it's lists.
	 */

	/* Choose appropriate list. */
	if(req->state == RS_HASHTABLE)
	{
		if (req->type == RT_READ) {
			hash_list = &req_file->related_reads.list;
			req->globalinfo = &req_file->related_reads;
		} else {
			hash_list = &req_file->related_writes.list;
			req->globalinfo = &req_file->related_writes;
		}
	}
	else
	{
		if(req->type == RT_READ) {
			hash_list = &req_file->predicted_reads.list;
			req->globalinfo = &req_file->predicted_reads;
		} else {
			hash_list = &req_file->predicted_writes.list;
			req->globalinfo = &req_file->predicted_writes;
		}
	}
	req->globalinfo->current_size += req->io_data.len;

	/* Search for proper position for @req (the list is sorted). */
	insertion_place = hash_list;
	if (!agios_list_empty(hash_list)) {
		agios_list_for_each_entry(tmp, hash_list, related) {
			if ((tmp->io_data.offset > req->io_data.offset) ||
			    ((tmp->io_data.offset == req->io_data.offset) &&
			    (tmp->io_data.len > req->io_data.len))) {
				insertion_place = &(tmp->related);
				break;
			}
		}
	}
	
	/* Add @req to the list in sorted order. */
	if(req->state == RS_HASHTABLE)
	{
		if(!insert_aggregations(req, insertion_place->prev, hash_list))
			agios_list_add(&req->related, insertion_place->prev);
		else
			debug("included in an already existing virtual request");
			
	}
	else
		agios_list_add(&req->related, insertion_place->prev); //predicted requests are not really aggregated into virtual requests
		
	if((config_get_trace_predict()) && (req->state == RS_PREDICTED))
		agios_trace_predict_addreq(req);

}

/*
 * Function adds @req to hash table.
 *
 * Arguments:
 *	@req	- request to be added.
 *	@clnt	- structure describing calling client.
 *
 * Locking:
 *	Must NOT hold hastlist_locks[hash_val]
 */
unsigned long hashtable_add_req(struct request_t *req)
{
	unsigned long hash_val;

	VERIFY_REQUEST(req);
	
	hash_val = AGIOS_HASH_STR(req->file_id) % AGIOS_HASH_ENTRIES;	

	agios_mutex_lock(&hashlist_locks[hash_val]);

	__hashtable_add_req(req, hash_val);

	return hash_val;
}

/* Internal function (but called by I/O scheduler)
 * that removes @req from hash table.
 *
 * Locking:
 *	Must be holding hashlist_locks[hash_val]
 */
inline void __hashtable_del_req(struct request_t *req)
{
	agios_list_del(&req->related);
}

/*
 * Function removes @req from hash table.
 *
 * Locking:
 *	Must NOT be holding hashlist_locks[hash_val]
 */ 
void hashtable_del_req(struct request_t *req)
{
	unsigned long hash_val = AGIOS_HASH_FN(req->file_id) % AGIOS_HASH_ENTRIES;

	VERIFY_REQUEST(req);

	agios_mutex_lock(&hashlist_locks[hash_val]);
	
	__hashtable_del_req(req);

	agios_mutex_unlock(&hashlist_locks[hash_val]);
}

/*
 * Function locks hash table slot (collision list) identified by @index.
 * Used by I/O scheduler before iterating through collision list.
 *
 * Locking:
 *	Must NOT be holding hashlist_locks[hash_val]
 *	Must call hashtable_unlock(@index) later.
 */ 
struct agios_list_head *hashtable_lock(int index)
{
	agios_mutex_lock(&hashlist_locks[index]);
	return &hashlist[index];
}
struct agios_list_head *hashtable_trylock(int index)
{
	if(pthread_mutex_trylock(&hashlist_locks[index]) == 0)
		return &hashlist[index];
	else
		return NULL;
}
/*must hold the lock*/
struct agios_list_head *get_hashlist(int index)
{
	return &hashlist[index];	
}

/*
 * Function unlocks hash table slot (collision list) identified by @index.
 * Used by I/O scheduler after iterating through collision list.
 *
 * Locking:
 *	Must be holding hashlist_locks[hash_val]
 */ 
void hashtable_unlock(int index)
{
	agios_mutex_unlock(&hashlist_locks[index]);
}

#ifdef ORANGEFS_AGIOS
int agios_add_request(char *file_id, int type, long long offset, long len, int64_t data, struct client *clnt)
#else
int agios_add_request(char *file_id, int type, long long offset, long len, int data, struct client *clnt) //, int app_id
#endif
{
	struct request_t *req;
	struct timespec arrival_time;
	unsigned long hash;
	
	PRINT_FUNCTION_NAME;

	agios_gettime(&(arrival_time));  	

	req = request_constructor(file_id, type, offset, len, data, get_timespec2llu(arrival_time), RS_HASHTABLE);
	if (req) {
		if(scheduler_needs_hashtable)
		{
			hash = hashtable_add_req(req);
			hashlist_reqcounter[hash]++; //had to remove it from inside __hashtable_add_req because that function is also used for prediction requests. we do not want to count them as requests for the scheduling algorithms
			last_timestamp++;
			req->timestamp = last_timestamp;
			if(config_get_predict_request_aggregation())
				prediction_newreq(req);
		}
		else
			timeline_add_req(req);

		proc_stats_newreq(req);  //update statistics
		if(config_get_trace())
			agios_trace_add_request(req);  //trace this request arrival
		inc_current_reqnb(); //increase the number of current requests on the scheduler
		/* Signalize to the consumer thread that new request was added. */
#ifdef AGIOS_KERNEL_MODULE
		complete(&consumer.request_added);
#else
		agios_mutex_lock(&consumer.request_added_mutex);
		agios_cond_signal(&consumer.request_added_cond);
		agios_mutex_unlock(&consumer.request_added_mutex);
#endif

		if(scheduler_needs_hashtable)
			debug("current status. hashtable[%d] has %d requests, there are %d requests in the scheduler to %d files.", hash, hashlist_reqcounter[hash], get_current_reqnb(), get_current_reqfilenb());
		if(scheduler_needs_hashtable)
			agios_mutex_unlock(&hashlist_locks[hash]);
		else
			agios_mutex_unlock(&timeline_mutex);	

		return 0;		
	}		
	return -ENOMEM;
}

int hashtable_get_size(void)
{
	return AGIOS_HASH_ENTRIES;
}

