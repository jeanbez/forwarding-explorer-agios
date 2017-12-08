/* File:	SRTF.c
 * Created: 	2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the shortest remaining time first scheduling algorithm
 *
 * Contributors:
 *		Federal University of Rio Grande do Sul (UFRGS)
 *		INRIA France
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
#include "SRTF.h"
#include "request_cache.h"
#include "consumer.h"
#include "iosched.h"
#include "req_hashtable.h"
#include "common_functions.h"
#include "estimate_access_times.h"
#include "iosched.h"
#include "hash.h"
#include "agios_request.h"

int SRTF_check_queue(struct related_list_t *related_list, struct related_list_t *predicted_list, long int *min_size)
{
	if(related_list->current_size <= 0) //we dont have requests in this queue, it cannot be selected
		return 0;
	else
	{
		if((predicted_list->current_size == 0) || (predicted_list->current_size < related_list->current_size))
			predicted_list->current_size = related_list->current_size*2; //if we dont have predictions (or they were wrong), we assume we have half of the requests //WARNING: we can never use current_size of a predicted list to see if it is empty
		if((predicted_list->current_size - related_list->current_size) < *min_size)
			*min_size = predicted_list->current_size - related_list->current_size;
		return 1;			
	}
}

struct related_list_t *SRTF_select_a_queue(int *current_hash)
{
	int i;
	struct agios_list_head *reqfile_l;
	long int min_size = LONG_MAX;
	struct related_list_t *chosen_queue=NULL;
	struct request_file_t *req_file;
	int evaluated_reqfiles=0;
	int chosen_hash=0;
	
	for(i=0; i< AGIOS_HASH_ENTRIES; i++)
	{
		reqfile_l = hashtable_lock(i);
		
		agios_list_for_each_entry(req_file, reqfile_l, hashlist)
		{
			if((!agios_list_empty(&req_file->related_writes.list)) || (!agios_list_empty(&req_file->related_reads.list)))
			{	 
				debug("file %s, related_reads com %ld e related_writes com %ld, min_size is %ld\n", req_file->file_id, req_file->related_reads.current_size, req_file->related_writes.current_size, min_size);
				if((req_file->related_reads.current_size < 0) || (req_file->related_writes.current_size < 0))
				{
					printf("PANIC! current_size for file %s is %ld and %ld\n", req_file->file_id, req_file->related_reads.current_size, req_file->related_writes.current_size);
					exit(-1);			
				}
				evaluated_reqfiles++;

				if(SRTF_check_queue(&req_file->related_reads, &req_file->predicted_reads, &min_size))
				{
					debug("selecting from reads\n");
					chosen_queue = &req_file->related_reads;
					chosen_hash = i;
				}
				if(SRTF_check_queue(&req_file->related_writes, &req_file->predicted_writes, &min_size))
				{
					debug("selecting from writes\n");
					chosen_queue = &req_file->related_writes;
					chosen_hash = i;
				}
			}
		}
		hashtable_unlock(i);
		if(evaluated_reqfiles >= current_reqfilenb)
		{
			debug("went through all reqfiles, stopping \n");
			break;
		}

	}
	if(!chosen_queue)
	{
		return NULL;
	}
	else
	{
		*current_hash = chosen_hash;
		return chosen_queue;
	}
}

void SRTF(void *clnt)
{
	int SRTF_current_hash=0;
	struct related_list_t *SRTF_current_queue;
	struct request_t *req;
	short int update_time=0;



	while((current_reqnb > 0) && (update_time == 0))
	{
		/*1. find the shortest queue*/
		SRTF_current_queue = SRTF_select_a_queue(&SRTF_current_hash);

		if(SRTF_current_queue)
		{
	
			hashtable_lock(SRTF_current_hash);

			/*2. select its first request and process it*/	
			if(!agios_list_empty(&SRTF_current_queue->list))
			{
				req = agios_list_entry(SRTF_current_queue->list.next, struct request_t, related);
				if(req)
				{
					/*removes the request from the hastable*/
					__hashtable_del_req(req);
					/*sends it back to the file system*/
					update_time = process_requests(req, (struct client *)clnt, SRTF_current_hash);
					/*cleanup step*/
					generic_post_process(req);
				}
			}
			else
			{
				printf("PANIC! We selected a queue, but couldnt get the request from it\n");
				exit(-1);
			}

			hashtable_unlock(SRTF_current_hash);
		}
	}
	
}
