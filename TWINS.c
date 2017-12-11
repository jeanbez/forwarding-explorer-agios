/* File:	TWINS.c
 * Created: 	2016
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 * Collaborators:
 *		Jean Luca Bez <jlbez (at) inf.ufrgs.br>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the exclusive time window scheduling algorithm
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
#include "TWINS.h"
#include "request_cache.h"
#include "consumer.h"
#include "iosched.h"
#include "common_functions.h"
#include "req_timeline.h"
#include "hash.h"
#include "agios_request.h"
#include "agios_config.h"


static int exclusive_tw_first_req;
static int current_exclusive_tw;
static struct timespec window_start;

//returns 0 on success
int TWINS_init()
{
	exclusive_tw_first_req = 1; //we'll start the first time window only when the first request is selected for processing (in the future we might want to reset it every once in a while)
	current_exclusive_tw = 0; //the first id we will prioritize
	return 0;
}
void TWINS_exit()
{
}

void TWINS(void *clnt)
{
	short int update_time=0;
	struct request_t *req;
	long hash;
	
	
	PRINT_FUNCTION_NAME;
	//we are not locking the current_reqnb_mutex, so we could be using outdated information. We have chosen to do this for performance reasons
	while((current_reqnb > 0) && (update_time == 0))
	{
		timeline_lock();

		//do we need to setup the window, or did it end already?
		if(exclusive_tw_first_req) 
		{
			//we are going to start the first window!
			agios_gettime(&window_start);
			exclusive_tw_first_req = 0;
			current_exclusive_tw = 0;
		}
		else if(get_nanoelapsed(window_start) >= config_twins_window)
		{
			//we're done with this window, time to move to the next one
			agios_gettime(&window_start);
			current_exclusive_tw++;
			if(current_exclusive_tw >= app_timeline_size)
				current_exclusive_tw = 0; //round robin!
			debug("time is up, moving on to window %d", current_exclusive_tw);
		}
	
		//process requests!
		if(!(agios_list_empty(&(app_timeline[current_exclusive_tw])))) //we can only process requests from the current app_id
		{	
			//take request from the right queue
			req = agios_list_entry(app_timeline[current_exclusive_tw].next, struct request_t, related);
			//remove from the queue
			agios_list_del(&req->related);

			/*sends it back to the file system*/
			//we need the hash for this request's file id so we can update its stats 
			hash = AGIOS_HASH_STR(req->file_id) % AGIOS_HASH_ENTRIES;
			update_time = process_requests(req, (struct client *)clnt, hash);
		}
		else
		{
			//if there are no requests for this queue, we should sleep a little so we won't be in busy waiting
			//TODO (if we really need to)

		}

		timeline_unlock();

	}
}
