/* File:	req_timeline.c
 * Created: 	September 2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 * Collaborators:
 * 		Jean Luca Bez <jlbez (at) inf.ufrgs.br>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It implements the timeline data structure used by some schedulers to keep requests
 *		Further information is available at http://agios.bitbucket.org/
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
#include <stdlib.h>
#include "agios.h"
#include "agios_request.h"
#include "req_timeline.h"
#include "mylist.h"
#include "common_functions.h"
#include "request_cache.h"
#include "iosched.h"
#include "req_hashtable.h"
#include "hash.h"
#include "agios_config.h"


AGIOS_LIST_HEAD(timeline); //a linked list of requests
//a lock to access all timeline structures
#ifdef AGIOS_KERNEL_MODULE
static DEFINE_MUTEX(timeline_mutex);
#else
static pthread_mutex_t timeline_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

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


/*
 * adds a request to the timeline. The ordering will depend on the scheduling policy
 * Must have timeline_mutex
 */

void timeline_add_req(struct request_t *req, unsigned long hash, struct request_file_t *given_req_file)
{
	__timeline_add_req(req, hash, given_req_file, &timeline);
}

void __timeline_add_req(struct request_t *req, unsigned long hash_val, struct request_file_t *given_req_file, struct agios_list_head *this_timeline)
{
	struct request_file_t *req_file = given_req_file;
	struct request_t *tmp;
	int aggregated=0;
	int tw_priority;
	struct agios_list_head *insertion_place;

	if(!req_file) //if a req_file structure has been given, we are actually migrating from hashtable to timeline and will copy the request_file_t structures, so no need to create new. Also the request pointers are already set, and we don't need to use locks here
	{
		PRINT_FUNCTION_NAME;
		VERIFY_REQUEST(req);

		debug("adding request %lu %lu to file %s", req->io_data.offset, req->io_data.len, req->file_id);	

		/*find the file and update its informations if needed*/
		req_file = find_req_file(&hashlist[hash_val], req->file_id, req->state); //we store file information in the hashtable 
		if(req_file->first_request_time == 0)
			req_file->first_request_time = req->jiffies_64;
		if(req->type == RT_READ)
			req->globalinfo = &req_file->related_reads;
		else
			req->globalinfo = &req_file->related_writes;

		if(current_alg == NOOP_SCHEDULER) //we don't really include requests when using the NOOP scheduler, we just go through this function because we want request_file_t  structures for statistics
			return;  

	}

	
	//the time window scheduling algorithm separates requests into windows
	if (current_alg == config_tw_size) 
	{
		// Calculate the request priority
		tw_priority = req->jiffies_64 / config_tw_size * 32768 + req->tw_app_id; //32768 here is the maximum value app_id can assume  //TODO make sure this is exactly like presented in the paper

		// Find the position to insert the request
		agios_list_for_each_entry(tmp, this_timeline, related)
		{
			if (tmp->tw_priority > tw_priority) {
				agios_list_add(&req->related, tmp->related.prev);
				
				return;
			}
		}

		// If it was not inserted, insert the request in the proper position
		agios_list_add_tail(&req->related, this_timeline);

		return;
	} 

	//the TO-agg scheduling algorithm searches the queue for contiguous requests. If it finds any, then aggregate them.	
	if((current_alg == TIMEORDER_SCHEDULER) && (current_scheduler->max_aggreg_size > 1))
	{	
		agios_list_for_each_entry(tmp, this_timeline, related)
		{
			if(tmp->globalinfo == req->globalinfo) //same type and to the same file
			{
				if(tmp->reqnb < current_scheduler->max_aggreg_size) 
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

	if(!aggregated)
	{
		if((!given_req_file) || (current_alg == NOOP_SCHEDULER))  //if no request_file_t structure was given, this is a regular new request, so we add it to timeline (unless we are using NOOP which does not include requests in timeline)
		{
			debug("request is not aggregated, inserting in the timeline");
			agios_list_add_tail(&req->related, this_timeline); 
		}
		else //we are rebuilding this queue from the hashtable, so we need to make sure requests are ordered by time (and we are not using the time window algorithm, otherwise it would have called return already (see above)
		{
			debug("request not aggregated and we are reordering the timeline, so looking for its place");
			insertion_place = this_timeline;
			if(!agios_list_empty(this_timeline))
			{
				agios_list_for_each_entry(tmp, this_timeline, related)
				{
					if(tmp->timestamp > req->timestamp)
					{
						insertion_place = &(tmp->related);
						break;	
					}
				}
			}
			agios_list_add(&req->related, insertion_place->prev);
		}
	}

}

/* this function is called when migrating between two scheduling algorithms when both use timeline and one of them is the TIME_WINDOW. In this case, it is necessary to redo the timeline so requests will be processed in the new relevant order */
void reorder_timeline()
{
	struct agios_list_head *new_timeline;
	struct request_t *req, *aux_req=NULL;
	unsigned long hash;

	//initialize new timeline structure
	new_timeline = (struct agios_list_head *)agios_alloc(sizeof(struct agios_list_head));
	new_timeline->prev = new_timeline;
	new_timeline->next = new_timeline;

	//get all requests from the previous timeline and include in the new one
	agios_list_for_each_entry(req, &timeline, related)
	{
		if(aux_req)
		{
			hash = AGIOS_HASH_STR(aux_req->file_id) % AGIOS_HASH_ENTRIES;
			agios_list_del(&aux_req->related);
			__timeline_add_req(aux_req, hash, aux_req->globalinfo->req_file, new_timeline);	
		}
		aux_req = req;
	}	
	if(aux_req)
	{
		hash = AGIOS_HASH_STR(aux_req->file_id) % AGIOS_HASH_ENTRIES;
		agios_list_del(&aux_req->related);
		__timeline_add_req(aux_req, hash, aux_req->globalinfo->req_file, new_timeline);	
	}

	//redefine the pointers
	new_timeline->prev->next = &timeline;
	new_timeline->next->prev = &timeline;
	timeline.next = new_timeline->next;
	timeline.prev = new_timeline->prev;
	free(new_timeline);
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
struct request_t *timeline_oldest_req(unsigned long *hash)
{
	struct request_t *tmp;

	//PRINT_FUNCTION_NAME;
	
	if (agios_list_empty(&timeline)) {
		return 0;
	}

	tmp = agios_list_entry(timeline.next, struct request_t, related);
	agios_list_del(&tmp->related);
	*hash = AGIOS_HASH_STR(tmp->file_id) % AGIOS_HASH_ENTRIES;

	return tmp;
}

//initializes data structures
inline void timeline_init()
{
	init_agios_list_head(&timeline);
}
inline void timeline_cleanup()
{
	list_of_requests_cleanup(&timeline);
}
