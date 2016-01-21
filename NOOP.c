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


#include "NOOP.h"
#include "req_timeline.h"
#include "req_hashtable.h"
#include "request_cache.h"
#include "consumer.h"
#include "iosched.h"
#include "common_functions.h"
#include "mylist.h"

//we need to know which was the data structure used by the last scheduling algorithm so we know where to take requests from
static short int previous_needs_hashtable=0;
void set_noop_previous_needs_hashtable(short int value)
{
	previous_needs_hashtable=value;
}

short int process_all_requests_from_related_list(struct related_list_t *related_list, struct client *clnt, int hash)
{
	struct request_t *req, *aux_req=NULL;
	short int update_time = 0;
	
	agios_list_for_each_entry(req, &related_list->list, related)
	{
		if(aux_req)
		{
			update_time = process_requests(aux_req, clnt, hash);	
			generic_post_process(aux_req);
			if(update_time)
			{
				aux_req = NULL;
				break;
			}
		}
		aux_req = req;
	} 
	if(aux_req)
	{
		update_time = process_requests(aux_req, clnt, hash);	
		generic_post_process(aux_req);
	}
	return update_time;
}

//Usually NOOP means not having a schedule function. However, when we dynamically change from another algorithm to NOOP, we may still have requests on queue. So we just process all of them.
void NOOP(void *clnt)
{
	struct agios_list_head *list;
	struct request_t *req;
	short int update_time=0;
	short int stop_processing=0;
	unsigned long hash;

//	PRINT_FUNCTION_NAME;
	while(!stop_processing) //we give a change to new requests by locking and unlocking to every rquest
	{
		list = timeline_lock();
		stop_processing = agios_list_empty(list);
		if(!stop_processing) 
		{
			req = timeline_oldest_req(&hash);
			debug("NOOP is processing leftover requests %s %lu %lu", req->file_id, req->io_data.offset, req->io_data.len);
			update_time = process_requests(req, clnt, hash);
			generic_post_process(req);
			if(update_time)
			{
				debug("NOOP cleanup exiting without finishing because of some refresh period");
				timeline_unlock();	
				break; //we've changed to NOOP and then started cleaning the current data structures (that we are no longer feeding). But it is time to refresh something so we will stop
			}
		}
		timeline_unlock();	
	}
}

