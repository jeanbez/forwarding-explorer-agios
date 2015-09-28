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
static unsigned long long int performance_time; 
static unsigned long long int performance_size;
static pthread_mutex_t performance_mutex = PTHREAD_MUTEX_INITIALIZER;

unsigned long long int get_performance_size(void)
{
	unsigned long long int ret;
	agios_mutex_lock(&performance_mutex);
	ret = performance_size;
	agios_mutex_unlock(&performance_mutex);
	return ret;
}
double get_performance_bandwidth(void)
{
	double ret=0.0;
	long double tmp_time;
	agios_mutex_lock(&performance_mutex);
	if(performance_time > 0)
	{
		tmp_time = get_ns2s(performance_time);
		if(tmp_time > 0)
			ret = (performance_size) / tmp_time;
	}
	agios_mutex_unlock(&performance_mutex);
	return ret;
}
void reset_performance_counters(void)
{
	agios_mutex_lock(&performance_mutex);
	performance_time = 0;
	performance_size = 0;
	agios_mutex_unlock(&performance_mutex);
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
int agios_release_request(char *file_id, short int type, unsigned long int len, unsigned long int offset)
{
	struct request_file_t *req_file;
	unsigned long hash_val;
	struct agios_list_head *list;
	struct related_list_t *related;
	struct request_t *req;
	short int found=0;
	unsigned long int elapsed_time;

	PRINT_FUNCTION_NAME;

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
		performance_time += elapsed_time;
		performance_size += req->io_data.len;

		generic_cleanup(req);
	}
	//else //what to do if we can't find the request???? is this possible????

	//release data structure lock
	if(performance_needs_hashtable)
		hashtable_unlock(hash_val);
	else
		timeline_unlock();

	//if the current scheduling algorithm follows synchronous approach, we need to allow it to continue
	iosched_signal_synchronous();

	return 1;
}
