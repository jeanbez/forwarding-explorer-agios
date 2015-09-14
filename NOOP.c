/* File:	NOOP.c
 * Created: 	2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 * Collaborators:
 *		Jean Luca Bez <jlbez (at) inf.ufrgs.br>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the no operation scheduling algorithm
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
#include "req_timeline.h"
#include "req_hashtable.h"
#include "request_cache.h"
#include "consumer.h"
#include "iosched.h"
#include "NOOP.h"

//we need to know which was the data structure used by the last scheduling algorithm so we know where to take requests from
static short int previous_needs_hashtable=0;
void set_noop_previous_needs_hashtable(short int value)
{
	previous_needs_hashtable=value;
}

void process_all_requests_from_related_list(struct related_list_t *related_list, struct client_t *clnt, int hash)
{
	struct request_t *req, *aux_req=NULL;
	
	agios_list_for_each_entry(req, related_list, related)
	{
		if(aux_req)
		{
			process_requests(aux_req, clnt, hash);	
			generic_post_process(aux_req);
		}
		aux_req = req;
	} 
	if(aux_req)
	{
		process_requests(aux_req, clnt, hash);	
		generic_post_process(aux_req);
	}
}

//Usually NOOP means not having a schedule function. However, when we dynamically change from another algorithm to NOOP, we may still have requests on queue. So we just process all of them.
void NOOP(void *clnt)
{
	int i;
	struct agios_list_head *list;
	struct request_t *req;

	if(previous_needs_hashtable) //the previous scheduling algorithm uses hashtable, so that's where we need to take requests from
	{
		for(i=0; i < AGIOS_HASH_ENTRIES; i++)
		{
			list = hashtable_lock(i);
			agios_list_for_each_entry(req_file, list, hashlist)
			{
				process_all_requests_from_related_list(&req_file->related_writes, clnt, i);
				process_all_requests_from_related_list(&req_file->related_reads, clnt, i);
			}
			hashtable_unlock(i);	
		}
	}
	else //we are going to take requests from the timeline
	{
		list = timeline_lock();
		agios_list_for_each_entry(req, list, related)
		{
			process_requests(req, clnt, -1);
			generic_post_process(req);
		}
		timeline_unlock();	
	}
}

