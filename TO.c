/* File:	TO.c
 * Created: 	2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 * Collaborators:
 *		Jean Luca Bez <jlbez (at) inf.ufrgs.br>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the timeorder and timeorder with aggregation scheduling algorithms 
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
#include "TO.h"
#include "req_timeline.h"
#include "request_cache.h"
#include "consumer.h"
#include "iosched.h"
#include "common_functions.h"
#include "agios_request.h"

//simple timeorder: requests are processed in the order they were received (it is the same as the timeorder with aggregation, the only difference between them is in the add_request part
void timeorder(void *clnt)
{
	struct request_t *req;	
	short int update_time = 0;
	unsigned long hash;

	PRINT_FUNCTION_NAME;
	
	while((current_reqnb > 0) && (update_time == 0))
	{
		timeline_lock();
		req = timeline_oldest_req(&hash);
		if(req)
		{
			update_time = process_requests(req, (struct client *)clnt, hash); //we give -1 as the hash so the process_requests function will realize we are taking requests from the timeline, not from the hashtable	
			generic_post_process(req);
		}
		else
			debug("PANIC! We believe there are %d requests in timeline, but we cannot get one", get_current_reqnb());
		timeline_unlock();
	}
	PRINT_FUNCTION_EXIT;
}

void simple_timeorder(void *clnt)
{
	timeorder(clnt); //the difference between them is in the inclusion of requests, the processing is the same
}


