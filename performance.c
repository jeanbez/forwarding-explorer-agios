/* File:	performance.c
 * Created: 	2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 * Collaborators:
 *		Jean Luca Bez <jlbez (at) inf.ufrgs.br>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the release function, called by the user after processing requests.
 *		It keeps performance measurements and handles the synchronous approach.
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

#include <string.h>

#include "agios.h"
#include "request_cache.h"
#include "hash.h"
#include "req_hashtable.h"
#include "req_timeline.h"
#include "iosched.h"
#include "performance.h"
#include "common_functions.h"

unsigned long int agios_processed_reqnb; //processed requests counter. I've decided not to protect it with a mutex although it is used by two threads. The library's user calls the release function, where this variable is modified. The scheduling thread reads it in the process_requests function. Since it is not critical to have the most recent value there, no mutex.

/************************************************************************************************************
 * GLOBAL PERFORMANCE METRICS
 ************************************************************************************************************/
static unsigned long long int performance_time[PERFORMANCE_VALUES]; 
static unsigned long long int performance_size[PERFORMANCE_VALUES];
static unsigned long int performance_timestamps[PERFORMANCE_VALUES];
int performance_algs[PERFORMANCE_VALUES];
int performance_start, performance_end;
static pthread_mutex_t performance_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 
 * returns the total amount of accessed data (in the last PERFORMANCE_VALUES time periods)
 * each time period corresponds to one scheduling algorithm selection (dynamic).
 * used by the proc module to write the stats file
 * must NOT hold performance_mutex
 */
unsigned long long int agios_get_performance_size(void)
{
	unsigned long long int ret=0;
	int i;
	agios_mutex_lock(&performance_mutex);
	i = performance_start;
	while(i != performance_end)
	{
		ret += performance_size[i];
		i++;
		if(i >= PERFORMANCE_VALUES)
			i=0;
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
unsigned long long int agios_get_performance_time(void)
{
	unsigned long long int ret=0;
	int i;
	agios_mutex_lock(&performance_mutex);
	i = performance_start;
	while(i != performance_end)
	{
		ret += performance_time[i];
		i++;
		if(i >= PERFORMANCE_VALUES)
			i=0;
	}
	agios_mutex_unlock(&performance_mutex);
	return ret;
}
/*
 * returns an array with the last PERFORMANCE VALUES performance measurements
 * additionally, gives pointer to the array with the list of the last
 * PERFORMANCE_VALUES selected scheduling algorithms (start and end are the
 * indexes to access the circular arrays) 
 * used by the proc module to write the stats file
 * must NOT hold performance mutex
 */
double * agios_get_performance_bandwidth() 
{
	double *ret = malloc(sizeof(double)*PERFORMANCE_VALUES);
	long double tmp_time;
	int i;

	agios_mutex_lock(&performance_mutex);

	for(i = 0; i< PERFORMANCE_VALUES; i++)
	{
		ret[i] = 0.0;
		
		if(performance_time[i] > 0)
		{
			tmp_time = get_ns2s(performance_time[i]);
			if(tmp_time > 0)
				ret[i] = (performance_size[i]) / tmp_time;
		}
	}
	agios_mutex_unlock(&performance_mutex);
	return ret;
}
/*
 * returns the index of the most recent scheduling algorithm (the current one)
 */
inline int agios_performance_get_latest_index()
{
	int i;
	i = performance_end-1;
	if(i < 0)
		i = PERFORMANCE_VALUES-1;
	return i;
}
/*
 * returns the bandwidth observed so far with the current scheduling algorithm
 * must NOT hold performance mutex
 */
double agios_get_current_performance_bandwidth(void)
{
	double ret;
	int i;
	long double tmp_time;

	PRINT_FUNCTION_NAME;

	agios_mutex_lock(&performance_mutex);
	debug("going to calculate current bandwidth. start = %d, end = %d", performance_start, performance_end);
	i = agios_performance_get_latest_index();
	tmp_time = get_ns2s(performance_time[i]);
	if(tmp_time > 0)
		ret = (performance_size[i])/tmp_time;
	else
		ret = 0;
	agios_mutex_unlock(&performance_mutex);	
	debug("returning performance %.2f", ret);
	return ret;
}
/*
 * resets all performance counters
 * must NOT hold performance mutex
 */
void agios_reset_performance_counters(void)
{
	int i;
	agios_mutex_lock(&performance_mutex);
	for(i=0; i< PERFORMANCE_VALUES; i++)
	{
		performance_time[i] = 0;
		performance_size[i] = 0;
		performance_timestamps[i] = 0;
		performance_algs[i] = -1;
	}
	performance_start = performance_end = 0;
	agios_mutex_unlock(&performance_mutex);
}
/*
 * the arrays used to store performance data are circular, with two indexes to represent where the current
 * version of the array begins and ends. This is an auxiliar function to increment indexes (because a new
 * measurement was added) while respecting physical array limits 
 * must hold performance mutex
 */
void increment_performance_index()
{
	performance_end++;
	if(performance_end >= PERFORMANCE_VALUES)
		performance_end = 0;
	if(performance_end == performance_start)
	{
		performance_start++;
		if(performance_start >= PERFORMANCE_VALUES)
			performance_start = 0;
	}
	
}
/*
 * function called when a new scheduling algorithm is selected, to add a slot to it in the performance
 * data structures.
 * must NOT hold performance mutex 
 */
void performance_set_new_algorithm(int alg)
{
	struct timespec now;

	agios_mutex_lock(&performance_mutex);
	agios_gettime(&now);
	performance_timestamps[performance_end] = get_timespec2llu(now);
	performance_algs[performance_end] = alg;
	increment_performance_index();
	agios_processed_reqnb=0; 
	agios_mutex_unlock(&performance_mutex);
}
/* 
 * function called when a request was released by the user (meaning it was already executed).
 * it returns the index among the performance arrays corresponding to the scheduling algorithm
 * which was executing when this request was sent for processing (because it makes no sense to
 * account this performance measurement to an algorithm which was not responsible for deciding
 * the execution of this request).
 * must hold performance mutex
 */
int get_request_timestamp_index(struct request_t *req)
{
	int i;
	
	PRINT_FUNCTION_NAME;
	i = agios_performance_get_latest_index();
	while(i != performance_start)
	{
		if(req->dispatch_timestamp > performance_timestamps[i])
			break;
		i--;
		if(i < 0)
			i = PERFORMANCE_VALUES-1;
	}
	PRINT_FUNCTION_EXIT;
	return i;
}
/* 
 * for debug
 * must hold performance mutex
 */
void print_all_performance_data()
{
	int i;

	debug("current situation of the performance model:");
	i = performance_start;
	while(i != performance_end)
	{
		if(performance_time[i] > 0)
			debug("%s - %llu bytes in %llu ns = %f bytes/s (timestamp %lu)", get_algorithm_name_from_index(performance_algs[i]), performance_size[i], performance_time[i], ((double)performance_size[i])/(((long double)performance_time[1])/1000000000L), performance_timestamps[i]);
		else
			debug("%s - %llu bytes in %llu ns (timestamp %lu)", get_algorithm_name_from_index(performance_algs[i]), performance_size[i], performance_time[i], performance_timestamps[i]);
		i++;
		if(i == PERFORMANCE_VALUES)
			i = 0;
	}
}


/************************************************************************************************************
 * RELEASE FUNCTION, CALLED BY THE USER AFTER PROCESSING A REQUEST
 ************************************************************************************************************/
int agios_release_request(char *file_id, short int type, unsigned long int len, unsigned long int offset)
{
	struct request_file_t *req_file;
	unsigned long hash_val;
	struct agios_list_head *list;
	struct related_list_t *related;
	struct request_t *req;
	short int found=0;
	unsigned long int elapsed_time;
	int index;
	short int previous_needs_hashtable;

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

	//find the structure for this file (first acquire lock)
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
		index = get_request_timestamp_index(req); //we use the timestamp from when the request was sent for processing, because we want to relate its performance to the scheduling algorithm who choose to process the request
		performance_time[index] += elapsed_time;
		performance_size[index] += req->io_data.len;

		if(index == agios_performance_get_latest_index())
		{
			agios_processed_reqnb++; //we only count it as a new processed request if it was issued by the current scheduling algorithm
			debug("a request issued by the current scheduling algorithm has come back! processed_reqnb is %lu", agios_processed_reqnb);
		}
#ifdef AGIOS_DEBUG
		print_all_performance_data();
#endif

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
