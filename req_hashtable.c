/* File:	req_hashtable.c
 * Created: 	September 2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It implements the hashtable data structure used by some schedulers to keep requests.
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
#include "agios.h"
#include "agios_request.h"
#include "req_hashtable.h"
#include "common_functions.h"
#include "hash.h"
#include "request_cache.h"
#include "trace.h"
#include "agios_config.h"


struct agios_list_head *hashlist; //a contiguous list of fixed size. Files are distributed among the positions in this list according to a hash function
int *hashlist_reqcounter = NULL; //how many requests are present in each position from the hashtable (used to speed the search for requests in the scheduling algorithms)
//one lock per hash position
#ifdef AGIOS_KERNEL_MODULE
static struct mutex *hashlist_locks;
#else
static pthread_mutex_t *hashlist_locks;
#endif

//initializes data structures and locks. returns 0 if success
int hashtable_init(void)
{
	int i;
	hashlist = (struct agios_list_head *) agios_alloc(sizeof(struct agios_list_head) * AGIOS_HASH_ENTRIES);
	if(!hashlist)
	{
		agios_print("AGIOS: cannot allocate memory for req cache\n");
		return -ENOMEM;
	}

	hashlist_locks = agios_alloc(SIZE_OF_AGIOS_MUTEX_T*(AGIOS_HASH_ENTRIES+1));
	if(!hashlist_locks)
	{
		agios_print("AGIOS: cannot allocate memory for req locks\n");
		agios_free(hashlist);
		return -ENOMEM;
	}

	hashlist_reqcounter = agios_alloc(sizeof(int)*(AGIOS_HASH_ENTRIES+1));
	if(!hashlist_reqcounter)
	{
		agios_print("AGIOS: cannot allocate memory for req counters\n");
		agios_free(hashlist);
		agios_free(hashlist_locks);
		return -ENOMEM;
	}

	
	for (i = 0; i < AGIOS_HASH_ENTRIES; i++) {
		init_agios_list_head(&hashlist[i]);
		agios_mutex_init(&(hashlist_locks[i]));
		hashlist_reqcounter[i]=0;
	}

	return 0;
}
void related_list_cleanup(struct related_list_t *related_list)
{
	list_of_requests_cleanup(&related_list->list);
	list_of_requests_cleanup(&related_list->dispatch);
}
void hashtable_cleanup(void)
{
	int i;
	struct request_file_t *req_file, *aux_req_file=NULL;


	for(i=0; i< AGIOS_HASH_ENTRIES; i++) //go through all lines of the hashtable
	{
		if(!agios_list_empty(&hashlist[i]))
		{
			agios_list_for_each_entry(req_file, &hashlist[i], hashlist) //go through all file structures
			{
				//go through all requests in the related lists
				related_list_cleanup(&req_file->related_reads);
				related_list_cleanup(&req_file->related_writes);
				related_list_cleanup(&req_file->predicted_reads);
				related_list_cleanup(&req_file->predicted_writes);
				if(req_file->file_id)
					free(req_file->file_id);
				
				if(aux_req_file)
				{
					agios_list_del(&aux_req_file->hashlist);
					agios_free(aux_req_file);
				}
				aux_req_file = req_file;
			} 
			if(aux_req_file)
			{
				agios_list_del(&aux_req_file->hashlist);
				agios_free(aux_req_file);
				aux_req_file = NULL;
			}

		}
	}

	agios_free(hashlist);
	agios_free(hashlist_locks);
	agios_free(hashlist_reqcounter);
	
}

/*
 * Function adds @req to hash table.
 *
 * Locking:
 * 	Must be holding hashlist_locks[hash_val]
 */
void hashtable_add_req(struct request_t *req, long hash_val, struct request_file_t *given_req_file)
{
	struct agios_list_head *hash_list = &hashlist[hash_val];
	struct request_file_t *req_file = given_req_file;
	struct request_t *tmp;
	struct agios_list_head *insertion_place;

#ifdef AGIOS_DEBUG
	if(req->state == RS_HASHTABLE)
		debug("adding request to file %s, offset %ld, size %ld", req->file_id, req->io_data.offset, req->io_data.len);
#endif

	/*finds the file to add to*/
	if(!req_file) //if a given file was given, we are migrating from timeline to hashtable. In this case, no need to create new request_file_t structures or to update any statistics
	{
		req_file = find_req_file(hash_list, req->file_id, req->state);
		//TODO deal req_file NULL error
	
		/*if it is the first request to this file (first actual request, anyway, it could have predicted requests only), we have to store the arrival time*/
		if(req->state != RS_PREDICTED)
		{
			/*see if this is the first request to the file*/
			if(req_file->first_request_time == 0)
				req_file->first_request_time = req->jiffies_64;
		}
	}

	/*
	 * Now we have request_file_t structure describing our file,
	 * so add @req to one of it's lists.
	 */

	/* Choose appropriate list. */
	if(req->state == RS_HASHTABLE)
	{
		if (req->type == RT_READ) {
			hash_list = &req_file->related_reads.list;
			req->globalinfo = &req_file->related_reads;
		} else {
			hash_list = &req_file->related_writes.list;
			req->globalinfo = &req_file->related_writes;
		}
	}
	else
	{
		if(req->type == RT_READ) {
			hash_list = &req_file->predicted_reads.list;
			req->globalinfo = &req_file->predicted_reads;
		} else {
			hash_list = &req_file->predicted_writes.list;
			req->globalinfo = &req_file->predicted_writes;
		}
	}

	/* Search for proper position for @req (the list is sorted). */
	insertion_place = hash_list;
	if (!agios_list_empty(hash_list)) {
		agios_list_for_each_entry(tmp, hash_list, related) {
			if ((tmp->io_data.offset > req->io_data.offset) ||
			    ((tmp->io_data.offset == req->io_data.offset) &&
			    (tmp->io_data.len > req->io_data.len))) {
				insertion_place = &(tmp->related);
				break;
			}
		}
	}
	
	/* Add @req to the list in sorted order. */
	if(req->state == RS_HASHTABLE)
	{
		if(!insert_aggregations(req, insertion_place->prev, hash_list))
			agios_list_add(&req->related, insertion_place->prev);
			
	}
	else
		agios_list_add(&req->related, insertion_place->prev); //predicted requests are not really aggregated into virtual requests
		
	if(req->state == RS_PREDICTED)
		if(config_trace_agios_predict)
			agios_trace_predict_addreq(req);

}

/* Internal function (but called by I/O scheduler)
 * that removes @req from hash table.
 *
 * Locking:
 *	Must be holding hashlist_locks[hash_val]
 */
void __hashtable_del_req(struct request_t *req)
{
	agios_list_del(&req->related);
}

/*
 * Function removes @req from hash table.
 *
 * Locking:
 *	Must NOT be holding hashlist_locks[hash_val]
 */ 
void hashtable_del_req(struct request_t *req)
{
	long hash_val = AGIOS_HASH_FN(req->file_id) % AGIOS_HASH_ENTRIES;

	VERIFY_REQUEST(req);

	agios_mutex_lock(&hashlist_locks[hash_val]);
	
	__hashtable_del_req(req);

	agios_mutex_unlock(&hashlist_locks[hash_val]);
}

/*
 * Function locks hash table slot (collision list) identified by @index.
 * Used by I/O scheduler before iterating through collision list.
 *
 * Locking:
 *	Must NOT be holding hashlist_locks[hash_val]
 *	Must call hashtable_unlock(@index) later.
 */ 
struct agios_list_head *hashtable_lock(int index)
{
	if((index < 0) || (index >= AGIOS_HASH_ENTRIES))
	{
		agios_print("PANIC! Trying to access line %d of the hashtable!", index);
		return NULL;
	}
	agios_mutex_lock(&hashlist_locks[index]);
	return &hashlist[index];
}
struct agios_list_head *hashtable_trylock(int index)
{
	if(pthread_mutex_trylock(&hashlist_locks[index]) == 0)
		return &hashlist[index];
	else
		return NULL;
}

/*
 * Function unlocks hash table slot (collision list) identified by @index.
 * Used by I/O scheduler after iterating through collision list.
 *
 * Locking:
 *	Must be holding hashlist_locks[hash_val]
 */ 
void hashtable_unlock(int index)
{
	agios_mutex_unlock(&hashlist_locks[index]);
}

void print_hashtable_line(int32_t i)
{
	struct agios_list_head *hash_list;
	struct request_file_t *req_file;
	struct request_t *req;

	hash_list = &hashlist[i];
	if(!agios_list_empty(hash_list))
		debug("[%d]", i);
	agios_list_for_each_entry(req_file, hash_list, hashlist)
	{
		debug("\t%s", req_file->file_id);
		if(!(agios_list_empty(&req_file->related_reads.list) && agios_list_empty(&req_file->related_reads.dispatch)))
		{
			debug("\t\tread");
			agios_list_for_each_entry(req, &req_file->related_reads.list, related)
				print_request(req);
			debug("\t\tdispatch read");
			agios_list_for_each_entry(req, &req_file->related_reads.dispatch, related)
				print_request(req);
		}
		if(!(agios_list_empty(&req_file->related_writes.list) && agios_list_empty(&req_file->related_writes.dispatch)))
		{
			debug("\t\twrite");
			agios_list_for_each_entry(req, &req_file->related_writes.list, related)
				print_request(req);
			debug("\t\tdispatch writes");
			agios_list_for_each_entry(req, &req_file->related_writes.dispatch, related)
				print_request(req);

		}
	}

}

//debug functions, clean after
void print_hashtable(void)
{
	int32_t i;

	debug("Current hashtable status:");
	for(i=0; i< AGIOS_HASH_ENTRIES; i++) //go through the whole hashtable, one position at a time
	{
		print_hashtable_line(i);
	}
	PRINT_FUNCTION_EXIT;
}
void print_timeline(void)
{
	struct request_t *req;

	debug("Current timeline status:");
	debug("Requests:");
	agios_list_for_each_entry(req, &timeline, related)
	{
		print_request(req);
	}
}
