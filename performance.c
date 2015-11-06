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

/************************************************************************************************************
 * GLOBAL PERFORMANCE METRICS
 ************************************************************************************************************/
static unsigned long long int performance_time[PERFORMANCE_VALUES]; 
static unsigned long long int performance_size[PERFORMANCE_VALUES];
static unsigned long int performance_timestamps[PERFORMANCE_VALUES];
static int performance_algs[PERFORMANCE_VALUES];
static int performance_start, performance_end;
static pthread_mutex_t performance_mutex = PTHREAD_MUTEX_INITIALIZER;

unsigned long long int get_performance_size(void)
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
unsigned long long int get_performance_time(void)
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

double * get_performance_bandwidth(int *start, int *end, int **algs) 
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
	*start = performance_start;
	*end = performance_end;
	*algs = performance_algs;
	agios_mutex_unlock(&performance_mutex);
	return ret;
}
double get_current_performance_bandwidth(void)
{
	double ret;
	int i;
	long double tmp_time;

	PRINT_FUNCTION_NAME;

	agios_mutex_lock(&performance_mutex);
	debug("going to calculate current bandwidth. start = %d, end = %d", performance_start, performance_end);
	i = performance_end-1;
	if(i < 0)
		i =0;
	tmp_time = get_ns2s(performance_time[i]);
	if(tmp_time > 0)
		ret = (performance_size[i])/tmp_time;
	else
		ret = 0;
	agios_mutex_unlock(&performance_mutex);	
	debug("returning performance %.2f", ret);
	return ret;
}
void reset_performance_counters(void)
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
void performance_set_new_algorithm(int alg)
{
	struct timespec now;

	agios_mutex_lock(&performance_mutex);
	agios_gettime(&now);
	performance_timestamps[performance_end] = get_timespec2llu(now);
	performance_algs[performance_end] = alg;
	increment_performance_index();
	agios_mutex_unlock(&performance_mutex);
}
int get_request_timestamp_index(struct request_t *req)
{
	int i;
	
	PRINT_FUNCTION_NAME;
	i = performance_end-1;
	if(i <0)
		i = PERFORMANCE_VALUES-1;
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

/************************************************************************************************************
 * LOCAL COPIES OF SCHEDULING ALGORITHM PARAMETERS
 ************************************************************************************************************/
static short int performance_needs_hashtable=0;
inline void performance_set_needs_hashtable(short int value)
{
	performance_needs_hashtable = value;
}

/************************************************************************************************************
 * RELEASE FUNCTION, CALLED BY THE USER AFTER PROCESSING A REQUEST
 ************************************************************************************************************/
static int release_count=0;
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

	PRINT_FUNCTION_NAME;

	lock_algorithm_migration_mutex();

	release_count++;
	debug("%d requests came back to us", release_count);
	//find the structure for this file (and acquire lock)
	if(performance_needs_hashtable)
	{
		hash_val = AGIOS_HASH_STR(file_id) % AGIOS_HASH_ENTRIES;	
		list = hashtable_lock(hash_val);
	}
	else
	{
		timeline_lock();
		list = get_timeline_files();
	}
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
		unlock_algorithm_migration_mutex();
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
//		agios_mutex_lock(&performance_mutex);
		//we need to figure out to each time slice this request belongs
		index = get_request_timestamp_index(req); //we use the timestamp from when the request was sent for processing, because we want to relate its performance to the scheduling algorithm who choose to process the request
		performance_time[index] += elapsed_time;
		performance_size[index] += req->io_data.len;
//		agios_mutex_unlock(&performance_mutex);

		generic_cleanup(req);
	}
	else
		debug("PANIC! Could not find the request %lu %lu to file %s\n", offset, len, file_id);
	//else //what to do if we can't find the request???? is this possible????

	//release data structure lock
	if(performance_needs_hashtable)
		hashtable_unlock(hash_val);
	else
		timeline_unlock();

	//if the current scheduling algorithm follows synchronous approach, we need to allow it to continue
	iosched_signal_synchronous();
	unlock_algorithm_migration_mutex();
	PRINT_FUNCTION_EXIT;

	return 1;
}
