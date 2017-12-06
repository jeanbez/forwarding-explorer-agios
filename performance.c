/* File:	performance.c
 * Created: 	2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the release function, called by the user after processing requests.
 *		It keeps performance measurements and handles the synchronous approach.
 *		Further information is available at http://inf.ufrgs.br/~fzboito/agios.html
 *
 * Contributors:
 *		Federal University of Rio Grande do Sul (UFRGS)
 *		INRIA France
 *
 *		This program is distributed in the hope that it will be useful,
 * 		but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <string.h>

#include "agios.h"
#include "request_cache.h"
#include "hash.h"
#include "req_hashtable.h"
#include "req_timeline.h"
#include "iosched.h"
#include "performance.h"
#include "common_functions.h"
#include "agios_config.h"
#include "agios_request.h"

long int agios_processed_reqnb; //processed requests counter. I've decided not to protect it with a mutex although it is used by two threads. The library's user calls the release function, where this variable is modified. The scheduling thread reads it in the process_requests function. Since it is not critical to have the most recent value there, no mutex.

/************************************************************************************************************
 * GLOBAL PERFORMANCE METRICS
 ************************************************************************************************************/
struct performance_entry_t //information about one time period, corresponding to one scheduling algorithm selection
{
	long timestamp;	//timestamp of when we started this time period
	int alg; //scheduling algorithm in use in this time period
	long time; //the sum of time to process of every requests in this time period
	long size; //the sum of size of every request in this time period
	struct agios_list_head list; 	
};
static AGIOS_LIST_HEAD(performance_info); //the list with all information we are holding about performance measurements
static int performance_info_len=0; //how many entries in performance_info
struct performance_entry_t *current_performance_entry; //the latest entry to performance_info
static pthread_mutex_t performance_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 
 * returns the total amount of accessed data (in the last PERFORMANCE_VALUES time periods)
 * each time period corresponds to one scheduling algorithm selection (dynamic).
 * used by the proc module to write the stats file
 * must NOT hold performance_mutex
 */
long int agios_get_performance_size(void)
{
	long int ret=0;
	struct performance_entry_t *entry=NULL;

	agios_mutex_lock(&performance_mutex);
	agios_list_for_each_entry(entry, &performance_info, list)
	{
		ret += entry->size;
	}
	agios_mutex_unlock(&performance_mutex);
	return ret;
}

/* 
 * returns the total time to process all requests (sum) in the last PERFORMANCE_VALUES
 * time periods. Each time period corresponds to one scheduling algorithm selection 
 * (dynamic).
 * used by the proc module to write the stats file
 * must NOT hold performance_mutex
 */
long int agios_get_performance_time(void)
{
	long int ret=0;
	struct performance_entry_t *entry=NULL;

	agios_mutex_lock(&performance_mutex);
	agios_list_for_each_entry(entry, &performance_info, list)
	{
		ret += entry->time;
	}
	agios_mutex_unlock(&performance_mutex);
	return ret;
}

/*
 * returns an array with the last PERFORMANCE VALUES performance measurements
 * used by the proc module to write the stats file
 * must NOT hold performance mutex
 */
double * agios_get_performance_bandwidth() 
{
	double *ret = malloc(sizeof(double)*config_agios_performance_values);
	int i=0;
	struct performance_entry_t *entry;
	if(!ret)
		return NULL;

	agios_mutex_lock(&performance_mutex);
	agios_list_for_each_entry(entry, &performance_info, list)
	{
		if(entry->time > 0) 
			ret[i] = ((double) entry->size) / get_ns2s(entry->time);
		else
			ret[i] = 0.0;
		i++;
	}
	agios_mutex_unlock(&performance_mutex);

	for(; i<config_agios_performance_values;i++) //it is possible we have less entries than expected
		ret[i] = 0.0;

	return ret;
}
/*
 * returns the bandwidth observed so far with the current scheduling algorithm
 * must NOT hold performance mutex
 */
double agios_get_current_performance_bandwidth(void)
{
	double ret;

	PRINT_FUNCTION_NAME;

	agios_mutex_lock(&performance_mutex);
	if(current_performance_entry->time > 0)
		ret = ((double) current_performance_entry->size) / get_ns2s(current_performance_entry->time);
	else
		ret = 0.0;
	agios_mutex_unlock(&performance_mutex);	
	return ret;
}
//returns 0 on success
int agios_performance_init(void)
{
	return 0;
}
/*
 * function called when a new scheduling algorithm is selected, to add a slot to it in the performance
 * data structures.
 * must NOT hold performance mutex 
 */
void performance_set_new_algorithm(int alg)
{
	
	struct timespec now;
	struct performance_entry_t *new = malloc(sizeof(struct performance_entry_t));
	if(!new)
	{
		agios_print("PANIC! could not allocate memory for the performance module!");
		return; //TODO what now?
	}
	new->time = 0;
	new->size = 0;
	agios_gettime(&now);
	new->timestamp = get_timespec2llu(now);
	new->alg = alg;
	
	agios_mutex_lock(&performance_mutex);
	
	agios_list_add_tail(&(new->list), &performance_info);
	current_performance_entry = new;
	performance_info_len++;
	agios_processed_reqnb=0; 

	//need to check if we dont have too many entries
	while(performance_info_len > config_agios_performance_values)
	{
		agios_list_for_each_entry(new, &performance_info, list) //we reuse the new variable since we are no longer using it to hold the new entry (it is already in the list)
			break; //this is to get the first entry of the list so we can remove it
		agios_list_del(&new->list); //remove the first of the list 
		performance_info_len--;
	}
	
	agios_mutex_unlock(&performance_mutex);
}

/* 
 * function called when a request was released by the user (meaning it was already executed).
 * it returns the entry of the scheduling algorithm that was executing when this request was 
 * sent for processing (because it makes no sense to
 * account this performance measurement to an algorithm which was not responsible for deciding
 * the execution of this request).
 * must hold performance mutex
 */
struct performance_entry_t * get_request_entry(struct request_t *req)
{
	struct performance_entry_t *ret = current_performance_entry;
	int found=0;

	do
	{
		if(ret->timestamp > req->dispatch_timestamp) //not this period
		{
			if(ret->list.prev == &performance_info) //end of the list
				break;
			ret = agios_list_entry(ret->list.prev, struct performance_entry_t, list); //go to the previous period	
		}
		else
		{
			found = 1;
			break;
		}
	} while(1);
	if(found == 0)
		ret = NULL;
	return ret;
}
/* 
 * for debug
 * must hold performance mutex
 */
void print_all_performance_data()
{
	struct performance_entry_t *aux;

	debug("current situation of the performance model:");

	agios_list_for_each_entry(aux, &performance_info, list)
	{
		if(aux->time > 0)
			debug("%s - %llu bytes in %llu ns = %.6f bytes/s (timestamp %lu)", get_algorithm_name_from_index(aux->alg), aux->size, aux->time, ((double)aux->size)/get_ns2s(aux->time), aux->timestamp);
		else
			debug("%s - %llu bytes in %llu ns (timestamp %lu)", get_algorithm_name_from_index(aux->alg), aux->size, aux->time, aux->timestamp);
	}
}


/************************************************************************************************************
 * RELEASE FUNCTION, CALLED BY THE USER AFTER PROCESSING A REQUEST
 ************************************************************************************************************/
int agios_release_request(char *file_id, short int type, long int len, long int offset)
{
	struct request_file_t *req_file;
	long hash_val;
	struct agios_list_head *list;
	struct related_list_t *related;
	struct request_t *req;
	short int found=0;
	long int elapsed_time;
	short int previous_needs_hashtable;
	struct performance_entry_t *entry;

	PRINT_FUNCTION_NAME;


	//first acquire lock
	hash_val = AGIOS_HASH_STR(file_id) % AGIOS_HASH_ENTRIES;
	while(1)
	{	
		previous_needs_hashtable = current_scheduler->needs_hashtable;
		if(previous_needs_hashtable)
			hashtable_lock(hash_val);
		else
			timeline_lock();

		if(previous_needs_hashtable != current_scheduler->needs_hashtable) //acquiring the lock means a memory wall, so we are sure to get the latest version of current_scheduler
		{
			//the other thread has migrated scheduling algorithms (and data structure) while we were waiting for the lock (so the lock is no longer the adequate one)
			if(previous_needs_hashtable)
				hashtable_unlock(hash_val);
			else
				timeline_unlock();
		}
		else
			break;
	}
	list = &hashlist[hash_val];

	//find the structure for this file 
	agios_list_for_each_entry(req_file, list, hashlist)
	{
		if(strcmp(req_file->file_id, file_id) == 0)
		{
			found = 1;
			break;
		}
	}
	if(found == 0)
	{
		//that makes no sense, we are trying to release a request which was never added!!!
		debug("PANIC! We cannot find the file structure for this request %s", file_id);
		return 0;
	}
	found = 0;
#ifdef AGIOS_DEBUG
	debug("Releasing a request from file %s:", req_file->file_id );
	print_hashtable_line(hash_val);
#endif

	//get the relevant list 
	if(type == RT_WRITE)
		related = &req_file->related_writes;
	else
		related = &req_file->related_reads;

	//find the request in the dispatch queue
	agios_list_for_each_entry(req, &related->dispatch, related)
	{
		if((req->io_data.len == len) && (req->io_data.offset == offset))
		{
			found =1;
			break;
		}
	}
	if(found)
	{
		//let's see how long it took to process this request
		elapsed_time = get_nanoelapsed_llu(req->jiffies_64);

		//update local performance information (we don't update processed_req_size here because it is updated in the generic_cleanup function)
		related->stats_window.processed_req_time += elapsed_time;
		related->stats_file.processed_req_time += elapsed_time;

		//update global performance information
		agios_mutex_lock(&performance_mutex);
		//we need to figure out to each time slice this request belongs
		entry = get_request_entry(req); //we use the timestamp from when the request was sent for processing, because we want to relate its performance to the scheduling algorithm who choose to process the request
		if(entry) //maybe the request took so long to process we don't even have a record for the scheduling algorithm that issued it
		{
			entry->time += elapsed_time;
			entry->time += req->io_data.len;

			if(entry == current_performance_entry)
			{
				agios_processed_reqnb++; //we only count it as a new processed request if it was issued by the current scheduling algorithm
				debug("a request issued by the current scheduling algorithm has come back! processed_reqnb is %lu", agios_processed_reqnb);
			}
		}

		agios_mutex_unlock(&performance_mutex);

		generic_cleanup(req);
	}
	else
		debug("PANIC! Could not find the request %lu %lu to file %s\n", offset, len, file_id);
	//else //what to do if we can't find the request???? is this possible????

	//release data structure lock
	if(current_scheduler->needs_hashtable)
		hashtable_unlock(hash_val);
	else
		timeline_unlock();

	//if the current scheduling algorithm follows synchronous approach, we need to allow it to continue
	iosched_signal_synchronous();
	PRINT_FUNCTION_EXIT;

	return 1;
}
