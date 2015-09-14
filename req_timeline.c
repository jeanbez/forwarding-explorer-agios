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
#include "agios.h"
#include "req_timeline.h"
#include "mylist.h"



static AGIOS_LIST_HEAD(timeline); //a linked list of requests
static AGIOS_LIST_HEAD(timeline_files); //a linked list of files being accessed (we need that for statistics
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

//must have timeline_mutex
inline struct agios_list_head *get_timeline_files(void)
{
	return &timeline_files;
}
//must have timeline mutex
inline struct agios_list_head *get_timeline(void)
{
	return &timeline;
}

/*
 * adds a request to the timeline. The ordering will depend on the scheduling policy
 * Must NOT have timeline_mutex
 * this function will lock the mutex, but will not unlock it before returning, so be sure to unlock it at some point after this function
 */

void timeline_add_req(struct request_t *req, int max_aggregation_size, int selected_alg, struct request_file_t *given_req_file)
{
	struct request_file_t *req_file = given_req_file;
	struct request_t *tmp;
	int aggregated=0;
	int tw_priority;

	if(!req_file) //if a req_file structure has been give, we are actually migrating from hashtable to timeline and will copy the request_file_t structures, so no need to create new. Also the request pointers are already set, and we don't need to use locks here
	{
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
	}
	
	if(selected_alg == NOOP_SCHEDULER) //we don't really include requests when using the NOOP scheduler, we just go through this function because we want request_file_t  structures for statistics
		return;  

	//the time window scheduling algorithm separates requests into windows
	if (selected_alg == TIME_WINDOW_SCHEDULER) 
	{
		// Calculate the request priority
		//TODO do you really want to use timestamp here? It is just a counter, you should probably use req->jiffies_64 and make sure TIME_WINDOW_SIZE is in ns
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

	//the TO-agg scheduling algorithm searches the queue for contiguous requests. If it finds any, then aggregate them.	
	if(selected_alg == TIMEORDER_SCHEDULER)
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

	if(!aggregated) //if not aggregated, possibly because this is the simple timeorder algorithm
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

	return tmp;
}

//initializes data structures
inline void timeline_init()
{
	init_agios_list_head(&timeline);
	init_agios_list_head(&timeline_files);
}
