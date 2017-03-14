/* File:	AIOLI.c
 * Created: 	2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the aIOLi scheduling algorithm
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
#include "AIOLI.h"
#include "request_cache.h"
#include "consumer.h"
#include "iosched.h"
#include "common_functions.h"
#include "req_hashtable.h"
#include "estimate_access_times.h"
#include "hash.h"
#include "agios_request.h"
#include "agios_config.h"


static int aioli_quantum; 
static struct timespec aioli_start;

int AIOLI_init()
{
	generic_init();
	aioli_quantum = 0;
	agios_gettime(&aioli_start);
	return 1;
}

int AIOLI_select_from_list(struct related_list_t *related_list, struct related_list_t **selected_queue, long int *selected_timestamp)
{
	int reqnb = 0;
	struct request_t *req;

	PRINT_FUNCTION_NAME;

	agios_list_for_each_entry(req, &related_list->list, related)
	{
		increment_sched_factor(req);
		if(&(req->related) == related_list->list.next) //we only try to select the first request from the queue (to respect offset order), but we don't break the loop because we want all requests to have their sched_factor incremented.
		{
			if(req->io_data.len <= req->sched_factor*config_aioli_quantum) //all requests start by a fixed size quantum (AIOLI_QUANTUM), which is increased every step (by increasing the sched_factor). The request can only be processed when its quantum is large enough to fit its size.
			{
				reqnb = req->reqnb;
				*selected_queue = related_list;
				*selected_timestamp = req->timestamp;
			}	
		}
	} 
	
	return reqnb;
}

int AIOLI_select_from_file(struct request_file_t *req_file, struct related_list_t **selected_queue, long int *selected_timestamp)
{
	int reqnb=0;
//	PRINT_FUNCTION_NAME;
	// First : on related read requests
	if (!agios_list_empty(&req_file->related_reads.list))
	{
		reqnb = AIOLI_select_from_list(&req_file->related_reads, selected_queue, selected_timestamp);
	}
	if((reqnb == 0) && (!agios_list_empty(&req_file->related_writes.list)))
	{
		reqnb = AIOLI_select_from_list(&req_file->related_writes, selected_queue, selected_timestamp);
	}
	return reqnb;
}

struct related_list_t *AIOLI_select_queue(int *selected_index)
{
	int i;
	struct agios_list_head *reqfile_l;
	struct request_file_t *req_file;
	int smaller_waiting_time=INT_MAX;	
	struct request_file_t *swt_file=NULL;
	int reqnb;
	struct related_list_t *tmp_selected_queue=NULL;
	struct related_list_t *selected_queue = NULL;
	long int tmp_timestamp;
	long int selected_timestamp=INT_MAX;
	int waiting_options=0;
	struct request_t *req=NULL;

//	PRINT_FUNCTION_NAME;
		
	for(i=0; i< AGIOS_HASH_ENTRIES; i++) //try to select requests from all the files
	{
		reqfile_l = hashtable_lock(i);
		if(!agios_list_empty(reqfile_l))
		{
			agios_list_for_each_entry(req_file, reqfile_l, hashlist)
			{
				if(req_file->waiting_time > 0)
				{
					update_waiting_time_counters(req_file, &smaller_waiting_time, &swt_file );	
					if(req_file->waiting_time > 0)
						waiting_options++;
				}
				if(req_file->waiting_time <= 0)	
				{
					tmp_selected_queue=NULL;
					reqnb = AIOLI_select_from_file(req_file, &tmp_selected_queue, &tmp_timestamp );
					if(reqnb > 0)
					{
						if(tmp_timestamp < selected_timestamp) //FIFO between the different files
						{
							selected_timestamp = tmp_timestamp;
							selected_queue = tmp_selected_queue;
							*selected_index = i;
						}
					}	
				}
				
			}
		}
		hashtable_unlock(i);
	}
	if(selected_queue) //if we were able to select a queue
	{
		hashtable_lock(*selected_index);
		req = agios_list_entry(selected_queue->list.next, struct request_t, related); 
		if(!checkSelection(req, selected_queue->req_file))  //test to see if we should wait (the function returns NULL when we have to wait)
		{
			selected_queue = NULL;
		}
		hashtable_unlock(*selected_index);
	}
	else if(waiting_options) // we could not select a queue, because all the files are waiting. So we should wait
	{
		debug("could not avoid it, will have to wait %u", smaller_waiting_time);
		agios_wait(smaller_waiting_time, swt_file->file_id);	
		swt_file->waiting_time = 0;
	}
	return selected_queue;
}

/*return the next quantum considering how much of the last one was used*/
long long int adjust_quantum(long int elapsed_time, int quantum, short int type)
{
	long int requiredqt;
	long int max_bound;
	long int used_quantum_rate = (elapsed_time*100)/quantum;

	/*adjust the next quantum considering how much of the last one was really used*/
	
	if(used_quantum_rate >= 175) /*we used at least 75% more than what was given*/
	{
		requiredqt = quantum*2;
	}
	else if(used_quantum_rate >= 125) /*we used at least 25% more than what was given*/
	{
		requiredqt = (quantum*15)/10;
	}
	else if(used_quantum_rate >= 75) /*we used at least 75% of the given quantum*/
	{
		requiredqt = quantum;
	}
	else /*we used less than 75% of the given quantum*/
	{
		requiredqt = quantum/2;
	}
		
	if(requiredqt <= 0)
		requiredqt = ANTICIPATORY_VALUE(config_aioli_quantum,type);
	else	
	{
		max_bound = (get_access_time(config_aioli_quantum,type)*MAX_AGGREG_SIZE);
		if(requiredqt > max_bound)
			requiredqt = max_bound;
	}

	return requiredqt;
}


void AIOLI(void *clnt)
{
	struct related_list_t *AIOLI_selected_queue=NULL;
	int selected_hash = 0;
	struct request_t *req;
	long long int remaining=0;
	short int type;
	short int update_time=0;

	PRINT_FUNCTION_NAME;
	//we are not locking the current_reqnb_mutex, so we could be using outdated information. We have chosen to do this for performance reasons
	while((current_reqnb > 0) && (update_time == 0))
	{
		/*1. select a request*/
		AIOLI_selected_queue = AIOLI_select_queue(&selected_hash);
		if(AIOLI_selected_queue)
		{
			hashtable_lock(selected_hash);
			//here we assume the list is NOT empty. It makes sense, since the other thread could have obtained the mutex, but only to include more requests, which would not make the list empty. If we ever have more than one thread consuming requests, this needs to be ensured somehow.

			/*we selected a queue, so we process requests from it until the quantum rans out*/
			aioli_quantum = AIOLI_selected_queue->nextquantum;
			agios_gettime(&aioli_start);

			remaining=0;
			do
			{
				//get the first request from this queue (because we need to keep the offset order within each queue
				agios_list_for_each_entry(req, &(AIOLI_selected_queue->list), related)
					break;

				if((remaining > 0) && (get_access_time(req->io_data.len, req->type) > remaining)) //we are using leftover quantum, but the next request is not small enough to fit in this space, so we just stop processing requests from this queue
					break;
			
				type = req->type;
				/*removes the request from the hastable*/
				__hashtable_del_req(req);
				/*sends it back to the file system*/
				update_time = process_requests(req, (struct client *)clnt, selected_hash);
				/*cleanup step*/
				waiting_algorithms_postprocess(req);

				/*we need to wait until the rquest was processed before moving on*/
				if(!update_time) //unless we are changing algorithms
				{
					//we need to release the hashtable/timeline mutes while waiting because otherwise other thread will not be able to acquire it in the release_request function and signal us
					hashtable_unlock(selected_hash);
					iosched_wait_synchronous();
					hashtable_lock(selected_hash);
				}

				
				/*lets see if we expired the quantum*/	
				remaining = aioli_quantum - get_nanoelapsed(aioli_start);
			} while((!agios_list_empty(&AIOLI_selected_queue->list)) && (remaining > 0) && (update_time == 0));
			
			/*here we either ran out of quantum, or of requests. Adjust the next quantum to be given to this queue considering this*/
			if(remaining <= 0) /*ran out of quantum*/
			{		
				if(aioli_quantum == 0) //it was the first time executing from this queue, we don't have information enough to decide the next quantum this file should receive, so let's just give it a default value
				{
					AIOLI_selected_queue->nextquantum = ANTICIPATORY_VALUE(config_aioli_quantum, type);
				}
				else //we had a quantum and it was enough
				{
					AIOLI_selected_queue->nextquantum = adjust_quantum(aioli_quantum-remaining, aioli_quantum, type);
				}
			}
			else /*ran out of requests*/
			{
				if(update_time == 0) //if update_time is 1, we have stopped for this queue because it was time to refresh things, not because there were no more requests or quantum left. If we adjust quantum anyway, we would penalize this queue for no reason
					AIOLI_selected_queue->nextquantum = adjust_quantum(aioli_quantum-remaining, aioli_quantum, type);
			}


			hashtable_unlock(selected_hash);
		}
	}
}
