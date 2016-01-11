/* File:	MLF.c
 * Created: 	2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 * Collaborators:
 *		Jean Luca Bez <jlbez (at) inf.ufrgs.br>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the MLF scheduling algorithm
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

#ifndef AGIOS_KERNEL_MODULE
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#else
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/slab.h>
#endif


#include "agios.h"
#include "MLF.h"
#include "request_cache.h"
#include "consumer.h"
#include "common_functions.h"
#include "req_hashtable.h"
#include "estimate_access_times.h"
#include "iosched.h"


static int MLF_current_hash=0; 
static int *MLF_lock_tries=NULL;


int MLF_init()
{
	int i;

	PRINT_FUNCTION_NAME;

	generic_init();
	MLF_lock_tries = agios_alloc(sizeof(int)*(AGIOS_HASH_ENTRIES+1));
	if(!MLF_lock_tries)
	{
		agios_print("AGIOS: cannot allocate memory for MLF structures\n");
		return 0;
	}
	for(i=0; i< AGIOS_HASH_ENTRIES; i++)
		MLF_lock_tries[i]=0;
	
	return 1;
}

void MLF_exit()
{
	PRINT_FUNCTION_NAME;
	agios_free(MLF_lock_tries);
}

/*selects a request from a queue (and applies MLF on this queue)*/
struct request_t *applyMLFonlist(struct related_list_t *reqlist)
{
	int found=0;
	struct request_t *req, *selectedreq=NULL;

	agios_list_for_each_entry(req, &(reqlist->list), related)
	{
		/*first, increment the sched_factor. This must be done to ALL requests, every time*/
		increment_sched_factor(req);
		if(!found)
		{
			/*see if the request's quantum is large enough to allow its execution*/
			if((req->sched_factor*MLF_QUANTUM) >= req->io_data.len)
			{
				selectedreq = req; 
				found = 1; /*we select the first possible request because we want to process them by offset order, and the list is ordered by offset. However, we do not stop the for loop here because we still have to increment the sched_factor of all requests (which is equivalent to increase their quanta)*/
			}
		}
	}
	
	return selectedreq;
}

/*selects a request from the queues associated with the file req_file*/
inline struct request_t *MLF_select_request(struct request_file_t *req_file)
{
	struct request_t *req=NULL;

	if(!agios_list_empty(&req_file->related_reads.list))
	{
		req = applyMLFonlist(&(req_file->related_reads)); 
	}
	if((!req) && (!agios_list_empty(&req_file->related_writes.list)))
	{
		req = applyMLFonlist(&(req_file->related_writes));
	}
	if(req)
		req = checkSelection(req, req_file);
	return req;
	//TODO consistency?
}

/*main function for the MLF scheduler. Selects requests, processes and then cleans up them. Returns only after consuming all requests*/
void MLF(void *clnt)
{	
	struct request_t *req;
	struct agios_list_head *reqfile_l;
	struct request_file_t *req_file;
	unsigned int smaller_waiting_time=~0;	
	struct request_file_t *swt_file=NULL;
	int starting_hash = MLF_current_hash;
	int processed_requests = 0;
	short int update_time=0;

	PRINT_FUNCTION_NAME;

	/*search through all the files for requests to process*/
	while((current_reqnb > 0) && (update_time == 0)) //attention: it could be outdated info (current_reqnb) since we are not using the mutex
	{
		/*try to lock the line of the hashtable*/
		reqfile_l = hashtable_trylock(MLF_current_hash);
		if(!reqfile_l) /*could not get the lock*/
		{
			if(MLF_lock_tries[MLF_current_hash] >= MAX_MLF_LOCK_TRIES)
				/*we already tried the max number of times, now we will wait until the lock is free*/
				reqfile_l = hashtable_lock(MLF_current_hash);
			else 
				MLF_lock_tries[MLF_current_hash]++;
		}
			
			
		if(reqfile_l) //if we got the lock
		{
			MLF_lock_tries[MLF_current_hash]=0;

			if(hashlist_reqcounter[MLF_current_hash] > 0) //see if we have requests for this line of the hashtable
			{
		                agios_list_for_each_entry(req_file, reqfile_l, hashlist)
				{
					/*do a MLF step to this file, potentially selecting a request to be processed*/
					/*but before we need to see if we are waiting new requests to this file*/
					if(req_file->waiting_time > 0)
					{
						update_waiting_time_counters(req_file, &smaller_waiting_time, &swt_file );
					}

					req = MLF_select_request(req_file);
					if((req) && (req_file->waiting_time <= 0))
					{
						/*removes the request from the hastable*/
						__hashtable_del_req(req);
						/*sends it back to the file system*/
						update_time = process_requests(req, (struct client *)clnt, MLF_current_hash);
						processed_requests++;
						/*cleanup step*/
						waiting_algorithms_postprocess(req);
						if(update_time)
							break; //get out of the agios_list_for_each_entry loop
					}
				}
			}
			hashtable_unlock(MLF_current_hash);
		}	
		if(update_time == 0) //if update time is 1, we've left the loop without going through all reqfiles, we should not increase the current hash yet
		{
			MLF_current_hash++;
			if(MLF_current_hash >= AGIOS_HASH_ENTRIES)
				MLF_current_hash = 0;
			if(MLF_current_hash == starting_hash) /*it means we already went through all the file structures*/
			{
				if((processed_requests == 0) && (swt_file))
				{
					/*if we can not process because every file is waiting for something, we have no choice but to sleep a little (the other choice would be to continue active waiting...)*/
					debug("could not avoid it, will have to wait %u", smaller_waiting_time);
					agios_wait(smaller_waiting_time, swt_file->file_id);	
					swt_file->waiting_time = 0;
					swt_file = NULL;
					smaller_waiting_time = ~0;
				}	
				processed_requests=0; /*restart the counting*/
			} 
		}
	}
}
