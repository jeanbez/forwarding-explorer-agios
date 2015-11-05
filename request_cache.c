
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

#include "req_hashtable.h"
#include "req_timeline.h"
#include "consumer.h"

#include "common_functions.h"
#include "NOOP.h"
#include "performance.h"

#ifdef AGIOS_KERNEL_MODULE
/*
 * Slab cache
 * It is used to manage frequently alocated structures:
 * request_t and request_file_t
 */
struct kmem_cache *request_cachep;
struct kmem_cache *request_file_cachep;
#endif

static short int scheduler_needs_hashtable=-1; //local copy of a configuration file parameter. I've had to declare it here because it is used in the counter functions

/**********************************************************************************************************************/
/*	COUNTERS	*/
/**********************************************************************************************************************/
static int current_reqnb; //request counter
static int current_reqfilenb; //files being accessed counter
static pthread_mutex_t current_reqnb_lock = PTHREAD_MUTEX_INITIALIZER; //lock to protect these counters
//we increase this number at every new request, just so each one of them has an unique identifier
static unsigned int last_timestamp=0; 

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
		dec_hashlist_reqcounter(hash);
}
/*must hold mutex to the hashtable line*/
inline void dec_many_current_reqnb(int hash, int value)
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_reqnb-= value;
	pthread_mutex_unlock(&current_reqnb_lock);
	if(scheduler_needs_hashtable)
		dec_many_hashlist_reqcounter(hash, value);
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


/**********************************************************************************************************************/
/*	LOCAL COPIES OF CONFIGURATION FILE PARAMETERS	*/
/**********************************************************************************************************************/
//these parameters are obtained from the configuration file at the beginning of the execution. We keep copies for performance reasons
static short int predict_request_aggregation=0;
static short int trace=0;
static unsigned int default_stripe_size=1;
inline void set_request_cache_predict_request_aggregation(short int value)
{
	predict_request_aggregation=value;
}
inline void set_request_cache_trace(short int value)
{
	trace = value;
}
inline void set_default_stripe_size(int value)
{
	default_stripe_size = value;
}

/**********************************************************************************************************************/
/*	SCHEDULING ALGORITHM PARAMETERS	*/
/**********************************************************************************************************************/
//parameters related to the current scheduling algorithm. 
//ATTENTION: these needs to be set up when changing scheduling algorithms
//TODO these probably belong in the iosched.c file, right? 
static int selected_alg = -1;
static int max_aggregation_size=-1;
static pthread_mutex_t algorithm_migration_mutex= PTHREAD_MUTEX_INITIALIZER; //this lock avoids that new requests are added while we are migrating between data structures because this could cause problems

inline int get_selected_alg()
{
	return selected_alg;
}
inline void set_selected_alg(int value) //use it only at first, when setting the parameters. DON'T use it for changing the current scheduling algorithm (use change_selected_alg instead)
{
	selected_alg = value;
}
inline void set_max_aggregation_size(int value) //use it only at first, when setting the parameters. DON'T use it for changing the current scheduling algorithm (use change_selected_alg instead)
{
	max_aggregation_size = value;
}
inline int get_max_aggregation_size()
{
	return max_aggregation_size;
}
inline short int get_needs_hashtable()
{
	return scheduler_needs_hashtable;
}
inline void set_needs_hashtable(short int value)
{
	scheduler_needs_hashtable = value;
}
inline void unlock_algorithm_migration_mutex(void)
{
	agios_mutex_unlock(&algorithm_migration_mutex);
}
inline void lock_algorithm_migration_mutex(void)
{
	agios_mutex_lock(&algorithm_migration_mutex);
}

void migrate_from_hashtable_to_timeline(int new_alg);
void migrate_from_timeline_to_hashtable(int new_alg);
//change the current scheduling algorithm and update local parameters
//here we assume the scheduling thread is NOT running, so it won't mess with the structures
//must hold the migration mutex
//must be called before config_gossip_algorithm_parameters (otherwise we won't know that something changed)
void change_selected_alg(int new_alg, short int new_needs_hashtable, int new_max_aggregation_size)
{	
	//TODO what about predict??

	//if we are moving to NOOP, we won't migrate data structures, we just need to process all requests already in the scheduler
//	if(new_alg == NOOP_SCHEDULER)
//	{
		//we are not going to process all requests from here because this would take a long time. The scheduler thread will take care of that, we just need to set the parameters
//		set_noop_previous_needs_hashtable(scheduler_needs_hashtable);
//	}
	//first situation: both algorithms use hashtable
//	else 
	if(scheduler_needs_hashtable && new_needs_hashtable)
	{
		//the only problem here is if we decreased the maximum aggregation
		//For now we chose to do nothing. If we no longer tolerate aggregations of a certain size, we are not spliting already performed aggregations since this would not benefit us at all. We could rethink that at some point
	}
	//second situation: from hashtable to timeline
	else if (scheduler_needs_hashtable && !new_needs_hashtable)
	{
		migrate_from_hashtable_to_timeline(new_alg);		
	}
	//third situation: from timeline to hashtable
	else if (!scheduler_needs_hashtable && new_needs_hashtable)
	{
		migrate_from_timeline_to_hashtable(new_alg);
	}
	//fouth situation, both algorithms use timeline
	else 
	{
		//now it depends on the algorithms. If we are going between the two timeorders, we don't have to do anything, since we have decided we will not split already performed aggregations. However, if we are going to or from the time window algorithm, we will need to reorder the whole list.
		if((new_alg != NOOP_SCHEDULER) && ((selected_alg == TIME_WINDOW_SCHEDULER) || (new_alg == TIME_WINDOW_SCHEDULER)))
		{
			reorder_timeline(new_alg, new_max_aggregation_size);
		}
	}

	selected_alg = new_alg;
	scheduler_needs_hashtable = new_needs_hashtable;
	max_aggregation_size = new_max_aggregation_size;
}

/**********************************************************************************************************************/
/*	FUNCTIONS TO CHANGE THE CURRENT DATA STRUCTURE BETWEEN HASHTABLE AND TIMELINE	*/
/**********************************************************************************************************************/
//re
void put_all_requests_in_timeline(struct agios_list_head *related_list, int new_alg, struct request_file_t *req_file, unsigned long hash)
{
	struct request_t *req, *aux_req=NULL;
	
	agios_list_for_each_entry(req, related_list, related)
	{
		if(aux_req)
		{
			agios_list_del(&aux_req->related);	
			timeline_add_req(aux_req, 1, new_alg, req_file); //we give 1 as max aggregation size because it is not necessary to try to aggregate requests, as they are already aggregated. We could have a difference in max_aggregation_size, but we have decided to make this change soft, i.e., it will affect new requests only 
		}
		aux_req = req;
		dec_hashlist_reqcounter(hash);
	}
	if(aux_req)
	{
		agios_list_del(&aux_req->related);
		timeline_add_req(aux_req, 1, new_alg, req_file);
	}
}
void put_req_in_hashtable(struct request_t *req)
{
	unsigned long hash;

	agios_list_del(&req->related);
	hash = hashtable_add_req(req, req->globalinfo->req_file);
	inc_hashlist_reqcounter(hash);
	
}
void put_req_file_in_timeline(struct request_file_t *req_file, struct agios_list_head *timeline_files)
{
	//TODO when using predict, we will continue to use the request_file_t structures in the hashtable, so maybe we cannot remove them, but just copy the used parts
	struct agios_list_head *insertion_place;
	struct request_file_t *other_req_file;

	agios_list_del(&req_file->hashlist); //remove from hashtable
	//find the right place in the timeline_files list (sorted by file_id)
	insertion_place = timeline_files; 
	agios_list_for_each_entry(other_req_file, timeline_files, hashlist)
		if(strcmp(other_req_file->file_id, req_file->file_id) >= 0)
		{
			insertion_place = &other_req_file->hashlist;	
			break;
		}
	//add it to timeline_files
	agios_list_add_tail(&req_file->hashlist, insertion_place);
}
void put_req_file_in_hashtable(struct request_file_t *req_file)
{
	//TODO when using predict, we will have request_file_t structures at the hashtable already. So when copying, we need to check if it already exists and then merge them
	unsigned long hash;
	struct agios_list_head *hash_list;
	struct agios_list_head *insertion_place;
	struct request_file_t *tmp_req_file;

	//remove from current location
	agios_list_del(&req_file->hashlist);

	hash = AGIOS_HASH_STR(req_file->file_id) % AGIOS_HASH_ENTRIES;
	hash_list = get_hashlist(hash);
	
	//find the position to put new req_file structure
	insertion_place = hash_list;
	agios_list_for_each_entry(tmp_req_file, hash_list, hashlist)
	{
		if(strcmp(req_file->file_id, tmp_req_file->file_id) >= 0)
		{
			insertion_place = &tmp_req_file->hashlist;
			break;
		}
	}
	agios_list_add_tail(&req_file->hashlist, insertion_place);
}
void put_all_req_file_in_hashtable(struct agios_list_head *timeline_files)
{
	struct request_file_t *req_file, *aux_req_file=NULL;
	agios_list_for_each_entry(req_file, timeline_files, hashlist)
	{
		if(aux_req_file)
			put_req_file_in_hashtable(aux_req_file);
		aux_req_file = req_file;
	}
	if(aux_req_file)
		put_req_file_in_hashtable(aux_req_file);
}
//gets all requests from hashtable and move them to the timeline. Also move all request_file_t structures to the timeline_files list. No need to hold mutexes, but NO OTHER THREAD may be using any of these data structures.
void migrate_from_hashtable_to_timeline(int new_alg)
{	
	int i;
	struct agios_list_head *hash_list;
	struct request_file_t *req_file, *aux_req_file=NULL;
	struct agios_list_head *timeline_files;

	//we will mess with the data structures and don't even use locks, since here we are certain no one else is messing with them
	timeline_files = get_timeline_files();
	for(i=0; i< AGIOS_HASH_ENTRIES; i++) //go through the whole hashtable, one position at a time
	{
		hash_list = get_hashlist(i);
		agios_list_for_each_entry(req_file, hash_list, hashlist)
		{	
			if(aux_req_file)
				//move the request_file_t structure from the hashtable to the timeline
				put_req_file_in_timeline(aux_req_file, timeline_files);
		
			//get all requests from it and put them in the timeline
			put_all_requests_in_timeline(&req_file->related_writes.list, new_alg, req_file, i);
			put_all_requests_in_timeline(&req_file->related_reads.list, new_alg, req_file, i);

			aux_req_file = req_file;
		}
		if(aux_req_file)
			put_req_file_in_timeline(aux_req_file, timeline_files);
	}
}
//gets al requests from timeline and move them to the hashtable. Also move all request_file_t structures to the hashtable to we keep statistics. No need to hold mutexes, but NO OTHER THREAD may be using any of these data structures
void migrate_from_timeline_to_hashtable(int new_alg)
{
	struct agios_list_head *timeline, *timeline_files;
	struct request_t *req, *aux_req=NULL;

	timeline = get_timeline();
	timeline_files = get_timeline_files();

	//move all request_file_t structures to the hashtable so they will already be there when we move the requests
	put_all_req_file_in_hashtable(timeline_files);

	//move all requests
	agios_list_for_each_entry(req, timeline, related)
	{
		if(aux_req)
		{
			put_req_in_hashtable(aux_req);	
		}
		aux_req = req;
	}
	if(aux_req)
		put_req_in_hashtable(aux_req);	
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
struct request_t * request_constructor(char *file_id, short int type, unsigned long int offset, unsigned long int len, int64_t data, unsigned long int arrival_time, short int state)
#else
struct request_t * request_constructor(char *file_id, short int type, unsigned long int offset, unsigned long int len, void * data, unsigned long int arrival_time, short int state) 
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
	last_timestamp++;
	new->timestamp = last_timestamp;

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
	stats->min_req_size = ~0;
	stats->max_req_size=0;

	stats->max_request_time = 0;
	stats->total_request_time = 0;
	stats->min_request_time = ~0;

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

	req_file->stripe_size = default_stripe_size;

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
 * This function allocates memory and initializes related locks 
 */
int request_cache_init(void)
{
	int ret=0;

	reset_global_reqstats(); //put all statistics to zero
	reset_performance_counters();

	timeline_init(); //initializes the timeline

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
		set_current_reqnb(0);
		set_current_reqfilenb(0);
	}

	agios_mutex_lock(&algorithm_migration_mutex); //so we won't try to add requests until the library finishes initializing and the scheduling algorithm is selected
	
	return ret;
}

/*
 * Function destroys all SLAB caches previously created by request_cache_init().
 */
void request_cache_cleanup(void)
{
	hashtable_cleanup();
}

//aggregation_head is a normal request which is about to become a virtual request upon aggregation with another contiguous request.
//for that, we need to create a new request_t structure to keep the virtual request, add it to the hashtable in place of aggregation_head, and include aggregation_head in its internal list
struct request_t *start_aggregation(struct request_t *aggregation_head, struct agios_list_head *prev, struct agios_list_head *next)
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
void include_in_aggregation(struct request_t *req, struct request_t **agg_req)
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
//we have two virtual requests which are going to become one because we've added a new one which fills the gap between them
void join_aggregations(struct request_t *head, struct request_t *tail)
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

#ifdef ORANGEFS_AGIOS
int agios_add_request(char *file_id, short int type, unsigned long int offset, unsigned long int len, int64_t data, struct client *clnt)
#else
int agios_add_request(char *file_id, short int type, unsigned long int offset, unsigned long int len, void * data, struct client *clnt) //TODO we will need app_id for time_window as well
#endif
{
	struct request_t *req;
	struct timespec arrival_time;
	unsigned long hash;
	
	PRINT_FUNCTION_NAME;
	agios_mutex_lock(&algorithm_migration_mutex);

	agios_gettime(&(arrival_time));  	

	req = request_constructor(file_id, type, offset, len, data, get_timespec2llu(arrival_time), RS_HASHTABLE);
	if (req) {
		if(scheduler_needs_hashtable)
		{
			hash = hashtable_add_req(req, NULL);
			inc_hashlist_reqcounter(hash); //had to remove it from inside __hashtable_add_req because that function is also used for prediction requests. we do not want to count them as requests for the scheduling algorithms
			if(predict_request_aggregation)
				prediction_newreq(req);
		}
		else
		{
			hash = -1;
			timeline_add_req(req, max_aggregation_size, selected_alg, NULL);
		}
		req->globalinfo->current_size += req->io_data.len;
		req->globalinfo->req_file->timeline_reqnb++;

		proc_stats_newreq(req);  //update statistics
		if(trace)
			agios_trace_add_request(req);  //trace this request arrival
		inc_current_reqnb(); //increase the number of current requests on the scheduler

		/* Signalize to the consumer thread that new request was added. */
		if(selected_alg != NOOP_SCHEDULER)
			consumer_signal_new_reqs();

		debug("current status: there are %d requests in the scheduler to %d files",get_current_reqnb(), get_current_reqfilenb());

		//if we are running the NOOP scheduler, we just process it already
		if(selected_alg == NOOP_SCHEDULER)
		{
			short int update_time;

			debug("NOOP is directly processing this request");
			update_time = process_requests(req, clnt, hash);
			generic_post_process(req);
			if(update_time)
				consumer_signal_new_reqs(); //if it is time to refresh something (change the scheduling algorithm or recalculate alpha), we wake up the scheduling thread
		}

		//free the lock
		if(scheduler_needs_hashtable)
			hashtable_unlock(hash);
		else
			timeline_unlock();	
		
	

		agios_mutex_unlock(&algorithm_migration_mutex);
		return 0;		
	}		
	agios_mutex_unlock(&algorithm_migration_mutex);
	return -ENOMEM;
}

//when agios is used to schedule requests to a parallel file system server, the stripe size is relevant to some calculations (specially for algorithm selection). A default value is provided through the configuration file, but many file systems (like PVFS) allow for each file to have different configurations. In this situation, the user could call this function to update a specific file's stripe size
int agios_set_stripe_size(char *file_id, unsigned int stripe_size)
{
	struct request_file_t *req_file;
	unsigned long hash_val;
	struct agios_list_head *list;

	//find the structure for this file (and acquire lock)
	if(scheduler_needs_hashtable)
	{
		hash_val = AGIOS_HASH_STR(file_id) % AGIOS_HASH_ENTRIES;	
		list = hashtable_lock(hash_val);
	}
	else
	{
		list = timeline_lock();
	}
	req_file = find_req_file(list, file_id, RS_HASHTABLE);
	req_file->stripe_size = stripe_size;
	return 1;
}
