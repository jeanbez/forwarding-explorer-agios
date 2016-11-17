/* File:	cancel_request.c
 * Created: 	2016 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the release function, called by the user after processing requests.
 *		It keeps performance measurements and handles the synchronous approach.
 *		Further information is available at http://agios.bitbucket.org/
 *
 * Contributors:
 *		Federal University of Rio Grande do Sul (UFRGS)
 *		INRIA France
 * 		Federal University of Santa Catarina (UFSC)
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
#include "agios_config.h"
#include "agios_request.h"

//removes a request from the scheduling queues
//returns 1 if success
int agios_cancel_request(char *file_id, short int type, unsigned long int len, unsigned long int offset)
{
	struct request_file_t *req_file;
	unsigned long hash_val;
	struct agios_list_head *list;
	struct request_t *req, *aux_req;
	short int found=0;
	short int previous_needs_hashtable;

	PRINT_FUNCTION_NAME;


	//first acquire lock
	hash_val = AGIOS_HASH_STR(file_id) % AGIOS_HASH_ENTRIES;
	while(1)
	{	
		previous_needs_hashtable = current_scheduler->needs_hashtable;
		if(previous_needs_hashtable)
			hashtable_lock(hash_val);
		else
			timeline_lock();

		if(previous_needs_hashtable != current_scheduler->needs_hashtable) //acquiring the lock means a memory wall, so we are sure to get the latest version of current_scheduler
		{
			//the other thread has migrated scheduling algorithms (and data structure) while we were waiting for the lock (so the lock is no longer the adequate one)
			if(previous_needs_hashtable)
				hashtable_unlock(hash_val);
			else
				timeline_unlock();
		}
		else
			break;
	}
	list = &hashlist[hash_val];

	//find the structure for this file 
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
		//that makes no sense, we are trying to cancel a request which was never added!!!
		debug("PANIC! We cannot find the file structure for this request %s", file_id);
		return 0;
	}
	found = 0;
#ifdef AGIOS_DEBUG
	debug("REMOVING a request from file %s:", req_file->file_id );
#endif

	//get the relevant list 
	if(previous_needs_hashtable)
	{
		if(type == RT_WRITE)
			list = &req_file->related_writes.list;
		else
			list = &req_file->related_reads.list;
	}
	else
		list = &timeline;

	//find the request
	agios_list_for_each_entry(req, list, related)
	{
		if(req->reqnb == 1) //simple request
		{
			if((req->io_data.len == len) && (req->io_data.offset == offset))
			{
				found =1;
				break;
			}
		}
		else //aggregated request, the one we're looking for could be inside it
		{
			if((req->io_data.offset <= offset) && (req->io_data.offset + req->io_data.len >= offset+len)) //no need to look if the request we're looking for is not inside this one
			{
				agios_list_for_each_entry(aux_req, &req->reqs_list, related)
				{
					if((aux_req->io_data.len == len) && (aux_req->io_data.offset == offset))
					{
						found = 1;
						break;
					}
				}
				if(found)
					break;
			}
		}
	}

	if(found)
	{
		//remove it from the queue
		if(req->reqnb == 1) //it's just a simple request
		{
			agios_list_del(&req->related);
		
			//the request is out of the queue, so now we update information about the file and request counters
			req->globalinfo->current_size -= req->io_data.len;
			req->globalinfo->req_file->timeline_reqnb--;
			if(req->globalinfo->req_file->timeline_reqnb == 0)
				dec_current_reqfilenb();
			dec_current_reqnb(hash_val);
	
 			//finally, free the structure
			request_cache_free(req);
		}
		else	//it's inside an aggregated request
		{
			int first;
			struct request_t *tmp;

			agios_list_del(&aux_req->related);
			//we need to update offset and len for the aggregated request without this one (and also timestamp)
			first = 1; //we will recalculate offset and len of the aggregation
			agios_list_for_each_entry(tmp, &req->reqs_list, related)
			{
				if(first)
				{
					first = 0;
					req->io_data.offset = tmp->io_data.offset;
					req->io_data.len = tmp->io_data.len;
					req->jiffies_64 = tmp->jiffies_64;
					req->timestamp = tmp->timestamp;
				}
				else
				{
					if(tmp->io_data.offset < req->io_data.offset)
					{
						req->io_data.len += req->io_data.offset - tmp->io_data.offset;
						req->io_data.offset = tmp->io_data.offset;
					}
					if((tmp->io_data.offset + tmp->io_data.len) > (req->io_data.offset + req->io_data.len))
					{
						req->io_data.len += (tmp->io_data.offset + tmp->io_data.len) - (req->io_data.offset + req->io_data.len);
					}
					if(tmp->jiffies_64 < req->jiffies_64)
						req->jiffies_64 = tmp->jiffies_64;
					if(tmp->timestamp < req->timestamp)
						req->timestamp = tmp->timestamp;
						
				}
				
			}
			//now let's update aggregated request information
			req->reqnb--;
			
			if(req->reqnb == 1) //it was aggregated, now it's not anymore
			{
				struct agios_list_head *prev, *next;

				//remove the virtual request from the queue and add its only request in its place
				prev = req->related.prev;
				next = req->related.next;
				agios_list_del(&req->related);
				tmp = agios_list_entry(req->reqs_list.next, struct request_t, related);
				__agios_list_add(&tmp->related, prev, next);
				request_cache_free(req);
			}
		
			//the request is out of the queue, so now we update information about the file and request counters
			aux_req->globalinfo->current_size -= aux_req->io_data.len;
			aux_req->globalinfo->req_file->timeline_reqnb--;
			if(aux_req->globalinfo->req_file->timeline_reqnb == 0)
				dec_current_reqfilenb();
			dec_current_reqnb(hash_val);
	
	 		//finally, free the structure
			request_cache_free(aux_req);
		}

			
	}
	else
		debug("PANIC! Could not find the request %lu %lu to file %s\n", offset, len, file_id);
		
	//release data structure lock
	if(previous_needs_hashtable)
		hashtable_unlock(hash_val);
	else
		timeline_unlock();

	return 1;
}
