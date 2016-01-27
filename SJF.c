/* File:	SJF.c
 * Created: 	2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 * Collaborators:
 *		Jean Luca Bez <jlbez (at) inf.ufrgs.br>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the shortest job first scheduling algorithm
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
#include "agios_request.h"
#include "SJF.h"
#include "request_cache.h"
#include "consumer.h"
#include "iosched.h"
#include "req_hashtable.h"
#include "common_functions.h"
#include "estimate_access_times.h"
#include "iosched.h"
#include "agios_config.h"
#include "hash.h"

inline int SJF_check_queue(struct related_list_t *related_list, unsigned long int min_size)
{
	if(related_list->current_size <= 0) //we dont have requests, cannot select this queue
		return 0; 
	else
	{
		if(related_list->current_size < min_size)
			return 1;
		else
			return 0;
	}
}

struct related_list_t *SJF_get_shortest_job(int *current_hash)
{
	int i;
	struct agios_list_head *reqfile_l;
	unsigned long int min_size = ULONG_MAX;
	struct related_list_t *chosen_queue=NULL;
	int chosen_hash=0;
	struct request_file_t *req_file;
	int evaluated_reqfiles=0;
	
	for(i=0; i< AGIOS_HASH_ENTRIES; i++)
	{
		reqfile_l = hashtable_lock(i);
		
		agios_list_for_each_entry(req_file, reqfile_l, hashlist)
		{
			if((!agios_list_empty(&req_file->related_writes.list)) || (!agios_list_empty(&req_file->related_reads.list)))
			{	 
				//debug("file %s, related_reads com %lu e related_writes com %lu, min_size is %lu\n", req_file->file_id, req_file->related_reads.current_size, req_file->related_writes.current_size, min_size);
				if((req_file->related_reads.current_size < 0) || (req_file->related_writes.current_size < 0))
				{
					printf("PANIC! current_size for file %s is %lu and %lu\n", req_file->file_id, req_file->related_reads.current_size, req_file->related_writes.current_size);
					exit(-1);			
				}
				evaluated_reqfiles++; //we count only files that have requests (others could have predicted requests but no actual ones)
				if((req_file->related_reads.current_size > 0) && (req_file->related_reads.current_size < min_size))
				{
				//	debug("selecting from reads\n");
					min_size = req_file->related_reads.current_size;
					chosen_queue = &req_file->related_reads;
					chosen_hash = i;
				}
				if((req_file->related_writes.current_size > 0) && (req_file->related_writes.current_size < min_size)) //TODO shouldn't it be else if?
				{
				//	debug("selecting from writes\n");
					min_size = req_file->related_writes.current_size;
					chosen_queue = &req_file->related_writes;
					chosen_hash = i;
				}
			}
		}
		hashtable_unlock(i);
		if(evaluated_reqfiles >= current_reqfilenb)
		{
		//	debug("went through all reqfiles, stopping \n");
			break;
		}

	}
	if(!chosen_queue)
	{
//		printf("PANIC! There are requests to be processed, but SJF cant pick a queue...\n");
//		exit(-1);
		return NULL;
	}
	else
	{
		*current_hash = chosen_hash;
		return chosen_queue;
	}
}
void SJF_postprocess(struct request_t *req)
{
	generic_post_process(req);
}

/*main function for the SJF scheduler. Selects requests, processes and then cleans up them. Returns only after consuming all requests*/
void SJF(void *clnt)
{	
	int SJF_current_hash=0;
	struct related_list_t *SJF_current_queue;
	struct request_t *req;
	short int update_time=0;



	while((current_reqnb > 0) && (update_time == 0))
	{
		/*1. find the shortest queue*/
		SJF_current_queue = SJF_get_shortest_job(&SJF_current_hash);

		if(SJF_current_queue)
		{
	
			hashtable_lock(SJF_current_hash);

			/*2. select its first request and process it*/	
			if(!agios_list_empty(&SJF_current_queue->list))
			{
				req = agios_list_entry(SJF_current_queue->list.next, struct request_t, related);
				if(req)
				{
					/*removes the request from the hastable*/
					__hashtable_del_req(req);
					/*sends it back to the file system*/
					update_time = process_requests(req, (struct client *)clnt, SJF_current_hash);
					/*cleanup step*/
					SJF_postprocess(req);
				}
			}
			else
			{
				printf("PANIC! We selected a queue, but couldnt get the request from it\n");
				exit(-1);
			}

			hashtable_unlock(SJF_current_hash);
		}
	}
}

