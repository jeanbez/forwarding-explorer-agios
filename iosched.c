/* File:	iosched.c
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the implementation for the five scheduling algorithms. 
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
#include "iosched.h"
#include "mylist.h"
#include "request_cache.h"
#include "common_functions.h"
#include "proc.h"
#include "hash.h"
#include "trace.h"
#include "predict.h"
#include "get_access_times.h"
#include "request_cache.h"
#include "consumer.h"
#include "agios_config.h"

/*for calculating alpha during execution, which represents the ability to overlap waiting time with processing other requests*/
static unsigned long long int time_spent_waiting=0;
static unsigned long long int waiting_time_overlapped=0;

inline unsigned long long int get_time_spent_waiting(void)
{
	return time_spent_waiting;
}
inline unsigned long long int get_waiting_time_overlapped(void)
{
	return waiting_time_overlapped;
}

/*for MLF scheduler*/
static int MLF_current_hash=0;
static int *MLF_lock_tries=NULL;
/*for aIOLi*/
static unsigned long long int aioli_quantum; 
static struct timespec aioli_start;


/*cleans up request_t structures after procesisng requests*/
void generic_post_process(struct request_t *req)
{
	struct request_t *sub_req, *aux_req=NULL;

	req->globalinfo->lastaggregation = req->reqnb;
	req->globalinfo->proceedreq_nb += req->reqnb;
	stats_aggregation(req->globalinfo);

	if(req->reqnb == 1)
		request_cache_free(req);
	else
	{
		agios_list_for_each_entry(sub_req, &(req->reqs_list), aggregation_element)
		{
			if(aux_req)
			{
				agios_list_del(&aux_req->aggregation_element);
				request_cache_free(aux_req);
				aux_req=NULL;
			}
			aux_req=sub_req;
		}
		if(aux_req)
		{
			agios_list_del(&aux_req->aggregation_element);
			request_cache_free(aux_req);
		}
	}
}
/* **** ***************** **** *
 * Simple 'Time order' scheduler
 * **** ***************** **** */
void timeorder(void *clnt)
{
	struct request_t *req;	
	
	while(get_current_reqnb() > 0)
	{
		timeline_lock();
		req = timeline_oldest_req();
		if(req)
		{
			process_requests(req, (struct client *)clnt, -1); //we give -1 as the hash so the process_requests function will realize we are taking requests from the timeline, not from the hashtable	
			generic_post_process(req);
		}
		timeline_unlock();
	}
}

void simple_timeorder(void *clnt)
{
	timeorder(clnt); //the difference between them is in the inclusion of requests, the processing is the same
}

/**************************************
 *
 * MLF scheduling algorithm
 *
 **************************************/


inline void increment_sched_factor(struct request_t *req)
{
	if(req->sched_factor == 0)
		req->sched_factor = 1;
	else
		req->sched_factor = req->sched_factor << 1;
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

/*see if we should wait before executing this request*/
inline struct request_t *checkSelection(struct request_t *req, struct request_file_t *req_file)
{
	struct request_t *retrn = req;


	/*waiting times are cause by 3 phenomena:*/
	/*1. shift phenomenon. One of the processes issuing requests to this queue is a little delayed, causing a contiguous request to arrive shortly after the other ones*/
	if(req->globalinfo->predictedoff != 0)
	{
		if(req->io_data.offset > req->globalinfo->predictedoff)
		{
			req_file->waiting_time = WAIT_SHIFT_CONST;
			stats_shift_phenomenon(req->globalinfo);
			if(config_get_trace())
			 	agios_trace_shift(WAIT_SHIFT_CONST, req->file_id);	
		}
		/*set to 0 to avoid starvation*/
		req->globalinfo->predictedoff = 0;
	} 

	/*2. better aggregation. If we just performed a larger aggregation on this queue, we believe we could do it again*/
	else if((req->io_data.offset > req->globalinfo->lastfinaloff) && (req->globalinfo->lastaggregation > req->reqnb))
	{
		/*set to zero to avoid starvation*/
		req->globalinfo->lastaggregation = 0;
		req_file->waiting_time = WAIT_AGGREG_CONST;
		stats_better_aggregation(req->globalinfo);
		if(config_get_trace())
			agios_trace_better(req->file_id);
	}
	/*3. we predicted that a better aggregation could be done to this request*/
	else if((config_get_predict_request_aggregation()) && ((req_file->waiting_time = agios_predict_should_we_wait(req)) > 0))
	{
		stats_predicted_better_aggregation(req->globalinfo);
		if(config_get_trace())
			agios_trace_predicted_better_aggregation(req_file->waiting_time, req->file_id);
	} 


	if(req_file->waiting_time)
	{
		retrn=NULL; /*we are no longer going to process the selected request, because we decided to wait*/
		agios_gettime(&req_file->waiting_start);
		time_spent_waiting += req_file->waiting_time;
	}
	return retrn;
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
	//TODO consistency
}

/*cleans up after processing a request*/
void MLF_postprocess(struct request_t *req)
{
	req->globalinfo->lastfinaloff = req->io_data.offset + req->io_data.len;	
	/*try to detect the shift phenomenon*/
	if((req->io_data.offset < req->globalinfo->laststartoff) && (!req->globalinfo->predictedoff))
	{
		req->globalinfo->predictedoff = req->globalinfo->lastfinaloff; 
	}	
	req->globalinfo->laststartoff = req->io_data.offset;

	generic_post_process(req);
}
/*sleeps for timeout ns*/
static void agios_wait(unsigned long long int  timeout, char *file)
{
#ifndef AGIOS_KERNEL_MODULE
	struct timespec timeout_tspec;
#endif


	if(config_get_trace())
		agios_trace_wait(timeout,file);
	
	debug("going to sleep for %llu\n", timeout);

#ifdef AGIOS_KERNEL_MODULE
	/*TODO see if this approach really works as expected*/
	set_current_state(TASK_INTERRUPTIBLE);
	ndelay(timeout);
	set_current_state(TASK_RUNNING);		
#else

	timeout_tspec.tv_sec = (unsigned long long int) timeout / 1000000000L;
	timeout_tspec.tv_nsec = (unsigned long long int) timeout % 1000000000L;

	nanosleep(&timeout_tspec, NULL);
#endif
}

void update_waiting_time_counters(struct request_file_t *req_file, unsigned long long int *smaller_waiting_time, struct request_file_t **swt_file )
{
	unsigned long long int elapsed;

	elapsed = get_nanoelapsed(req_file->waiting_start);
	if(req_file->waiting_time > elapsed)
	{
		req_file->waiting_time = req_file->waiting_time - elapsed;
		waiting_time_overlapped += elapsed;
		if(req_file->waiting_time < *smaller_waiting_time)
		{
			*smaller_waiting_time = req_file->waiting_time;
			*swt_file=req_file;
		}
	}
	else
	{
		waiting_time_overlapped+= req_file->waiting_time;
		req_file->waiting_time=0;
	}
	
}

/*main function for the MLF scheduler. Selects requests, processes and then cleans up them. Returns only after consuming all requests*/
void MLF(void *clnt)
{	
	struct request_t *req;
	struct agios_list_head *reqfile_l;
	struct request_file_t *req_file;
	unsigned long long int smaller_waiting_time=~0;	
	struct request_file_t *swt_file=NULL;
	int starting_hash = MLF_current_hash;
	int processed_requests = 0;

	PRINT_FUNCTION_NAME;

	/*search through all the files for requests to process*/
	while(get_current_reqnb() > 0)
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

			if(get_hashlist_reqcounter(MLF_current_hash) > 0) //see if we have requests for this line of the hashtable
			{
				debug("got the lock for hash %d", MLF_current_hash);

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
						process_requests(req, (struct client *)clnt, MLF_current_hash);
						processed_requests++;
						/*cleanup step*/
						MLF_postprocess(req);
					}
					else if(req_file->waiting_time > 0)
						debug("this file is waiting for now, so no processing");
				}
				debug("freeing the lock for hash %d", MLF_current_hash);
			}
			hashtable_unlock(MLF_current_hash);
		}	
		MLF_current_hash++;
		if(MLF_current_hash >= AGIOS_HASH_ENTRIES)
			MLF_current_hash = 0;
		if(MLF_current_hash == starting_hash) /*it means we already went through all the file structures*/
		{
			if((processed_requests == 0) && (swt_file))
			{
				/*if we can not process because every file is waiting for something, we have no choice but to sleep a little (the other choice would be to continue active waiting...)*/
				debug("could not avoid it, will have to wait %llu", smaller_waiting_time);
				agios_wait(smaller_waiting_time, swt_file->file_id);	
				swt_file->waiting_time = 0;
				swt_file = NULL;
				smaller_waiting_time = ~0;
			}	
			processed_requests=0; /*restart the counting*/
		} 
	}
}

static int MLF_init()
{
	int i;

	PRINT_FUNCTION_NAME;

	time_spent_waiting=0;
	waiting_time_overlapped=0;
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

static void MLF_exit()
{
	PRINT_FUNCTION_NAME;
	agios_free(MLF_lock_tries);
}

/**************************************
 *
 * aIOLi scheduling algorithm
 *
 **************************************/
static int AIOLI_init()
{
	time_spent_waiting=0;
	waiting_time_overlapped=0;
	aioli_quantum = 0;
	agios_gettime(&aioli_start);
	return 1;
}

int AIOLI_select_from_list(struct related_list_t *related_list, struct related_list_t **selected_queue, unsigned long long int *selected_timestamp)
{
	int reqnb = 0;
	struct request_t *req;

	PRINT_FUNCTION_NAME;
	agios_list_for_each_entry(req, &related_list->list, related)
	{
		increment_sched_factor(req);
		if(((req->sched_factor*AIOLI_QUANTUM*related_list->used_quantum_rate)/100) >= req->io_data.len)
		{
			reqnb = req->reqnb;
			*selected_queue = related_list;
			*selected_timestamp = req->timestamp;
		}
		break;
	} 
	if(reqnb > 0)
		debug("selected a request, reqnb = %d\n", reqnb);
	
	return reqnb;
}

int AIOLI_select_from_file(struct request_file_t *req_file, struct related_list_t **selected_queue, unsigned long long int *selected_timestamp)
{
	int reqnb=0;
	PRINT_FUNCTION_NAME;
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
	unsigned long long int smaller_waiting_time=~0;	
	struct request_file_t *swt_file=NULL;
	int reqnb;
	struct related_list_t *tmp_selected_queue=NULL;
	struct related_list_t *selected_queue = NULL;
	unsigned long long int tmp_timestamp, selected_timestamp=~0;
	int waiting_options=0;
	struct request_t *req=NULL;

	PRINT_FUNCTION_NAME;
		
	for(i=0; i< AGIOS_HASH_ENTRIES; i++) //try to select requests from all the files
	{
		reqfile_l = hashtable_lock(i);
		if(!agios_list_empty(reqfile_l))
		{
			agios_list_for_each_entry(req_file, reqfile_l, hashlist)
			{
				debug("looking for requests to file %s\n", req_file->file_id);
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
		debug("released the lock after selecting a request to a queue from file %s, timestamp %d\n", selected_queue->req_file->file_id, selected_timestamp);
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
		debug("could not avoid it, will have to wait %llu", smaller_waiting_time);
		agios_wait(smaller_waiting_time, swt_file->file_id);	
		swt_file->waiting_time = 0;
	}
	return selected_queue;
}

/*return the next quantum considering how much of the last one was used*/
inline unsigned long long int adjust_quantum(unsigned long long int elapsed_time, unsigned long long int quantum, long long lastsize, int type, struct related_list_t *globalinfo)
{
	unsigned long long int requiredqt;
	unsigned long long int max_bound;

	globalinfo->used_quantum_rate= (elapsed_time*100)/quantum;

	/*adjust the next quantum considering how much of the last one was really used*/
	
	if(globalinfo->used_quantum_rate >= 175) /*we used at least 75% more than what was given*/
	{
		requiredqt = quantum*2;
		globalinfo->used_quantum_rate *= 2;
	}
	else if(globalinfo->used_quantum_rate >= 125) /*we used at least 25% more than what was given*/
	{
		requiredqt = (quantum*15)/10;
		globalinfo->used_quantum_rate = (globalinfo->used_quantum_rate*15)/10;
	}
	else if(globalinfo->used_quantum_rate >= 75) /*we used at least 75% of the given quantum*/
	{
		requiredqt = quantum;
	}
	else if(globalinfo->used_quantum_rate > 25) /*we used more than 25% of the given quantum*/
	{
		requiredqt = quantum/2;
		globalinfo->used_quantum_rate = globalinfo->used_quantum_rate/2;
	}
	else //if(proportion <= 25)  /*we used so little that we should just reset our quantum system*/
	{
		requiredqt = quantum/4;
		globalinfo->used_quantum_rate = globalinfo->used_quantum_rate/4;
		
	}
		
	if(requiredqt <= 0)
		requiredqt = ANTICIPATORY_VALUE(type);
	else	
	{
		max_bound = (get_access_time(AIOLI_QUANTUM,type)*MAX_AGGREG_SIZE);
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
	int type;

	PRINT_FUNCTION_NAME;
	while(get_current_reqnb() > 0)
	{
		/*1. select a request*/
		AIOLI_selected_queue = AIOLI_select_queue(&selected_hash);
		if(AIOLI_selected_queue)
		{
			hashtable_lock(selected_hash);
			/*we selected a queue, so we process requests from it until the quantum rans out*/
			aioli_quantum = AIOLI_selected_queue->nextquantum;
			agios_gettime(&aioli_start);

			remaining=0;
			do
			{
				agios_list_for_each_entry(req, &(AIOLI_selected_queue->list), related)
					break;

				if((remaining > 0) && (get_access_time(req->io_data.len, req->type) > remaining)) //we are using leftover quantum, but the next request is not small enough to fit in this space
					break;
			
				type = req->type;
				/*removes the request from the hastable*/
				__hashtable_del_req(req);
				/*sends it back to the file system*/
				process_requests(req, (struct client *)clnt, selected_hash);
				debug("processed requests, quantum was %lld",aioli_quantum);
				/*cleanup step*/
				MLF_postprocess(req);
				
				/*lets see if we expired the quantum*/	
				remaining = aioli_quantum - get_nanoelapsed(aioli_start);
				debug("...remaining quantum is %lld", remaining);
				if(remaining <= 0) /*ran out of quantum*/
				{		
					if(aioli_quantum == 0) //it was the first time executing from this queue
					{
						AIOLI_selected_queue->nextquantum = ANTICIPATORY_VALUE(type);
					}
					else
					{
						AIOLI_selected_queue->nextquantum = adjust_quantum(aioli_quantum-remaining, aioli_quantum, AIOLI_selected_queue->lastfinaloff - AIOLI_selected_queue->laststartoff, type, AIOLI_selected_queue);
					}
				}
				else //we still have time left
				{
					AIOLI_selected_queue->nextquantum = adjust_quantum(aioli_quantum-remaining, aioli_quantum, AIOLI_selected_queue->lastfinaloff - AIOLI_selected_queue->laststartoff, type, AIOLI_selected_queue);
				}

			} while((!agios_list_empty(&AIOLI_selected_queue->list)) && (remaining > 0));

			hashtable_unlock(selected_hash);
		}
	}
}



/**************************************
 *
 * SJF scheduling algorithm
 *
 **************************************/
inline int SJF_check_queue(struct related_list_t *related_list, long long min_size)
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
	long long min_size = LLONG_MAX;
	struct related_list_t *chosen_queue=NULL;
	int chosen_hash=0;
	struct request_file_t *req_file;
	int evaluated_reqfiles=0;
	int reqfilenb = get_current_reqfilenb();
	
	for(i=0; i< AGIOS_HASH_ENTRIES; i++)
	{
		reqfile_l = hashtable_lock(i);
		
		agios_list_for_each_entry(req_file, reqfile_l, hashlist)
		{
			if((!agios_list_empty(&req_file->related_writes.list)) || (!agios_list_empty(&req_file->related_reads.list)))
			{	 
				debug("file %s, related_reads com %lld e related_writes com %lld, min_size is %lld\n", req_file->file_id, req_file->related_reads.current_size, req_file->related_writes.current_size, min_size);
				if((req_file->related_reads.current_size < 0) || (req_file->related_writes.current_size < 0))
				{
					printf("PANIC! current_size for file %s is %lld and %lld\n", req_file->file_id, req_file->related_reads.current_size, req_file->related_writes.current_size);
					exit(-1);			
				}
				evaluated_reqfiles++; //we count only files that have requests (others could have predicted requests but no actual ones)
				if((req_file->related_reads.current_size > 0) && (req_file->related_reads.current_size < min_size))
				{
					debug("selecting from reads\n");
					min_size = req_file->related_reads.current_size;
					chosen_queue = &req_file->related_reads;
					chosen_hash = i;
				}
				if((req_file->related_writes.current_size > 0) && (req_file->related_writes.current_size < min_size)) //TODO shouldn't it be else if?
				{
					debug("selecting from writes\n");
					min_size = req_file->related_writes.current_size;
					chosen_queue = &req_file->related_writes;
					chosen_hash = i;
				}
			}
		}
		hashtable_unlock(i);
		if(evaluated_reqfiles >= reqfilenb)
		{
			debug("went through all reqfiles, stopping \n");
			break;
		}

	}
	if(!chosen_queue)
	{
		printf("PANIC! There are requests to be processed, but SJF cant pick a queue...\n");
		exit(-1);
	}

	*current_hash = chosen_hash;
	return chosen_queue;
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



	while(get_current_reqnb() > 0)
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
					process_requests(req, (struct client *)clnt, SJF_current_hash);
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


/**************************************
 *
 * SRTF scheduling algorithm
 *
 **************************************/
int SRTF_check_queue(struct related_list_t *related_list, struct related_list_t *predicted_list, long long *min_size)
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
	long long min_size = LLONG_MAX;
	struct related_list_t *chosen_queue=NULL;
	struct request_file_t *req_file;
	int evaluated_reqfiles=0;
	int chosen_hash=0;
	int reqfilenb = get_current_reqfilenb();
	
	for(i=0; i< AGIOS_HASH_ENTRIES; i++)
	{
		reqfile_l = hashtable_lock(i);
		
		agios_list_for_each_entry(req_file, reqfile_l, hashlist)
		{
			if((!agios_list_empty(&req_file->related_writes.list)) || (!agios_list_empty(&req_file->related_reads.list)))
			{	 
				debug("file %s, related_reads com %lld e related_writes com %lld, min_size is %lld\n", req_file->file_id, req_file->related_reads.current_size, req_file->related_writes.current_size, min_size);
				if((req_file->related_reads.current_size < 0) || (req_file->related_writes.current_size < 0))
				{
					printf("PANIC! current_size for file %s is %lld and %lld\n", req_file->file_id, req_file->related_reads.current_size, req_file->related_writes.current_size);
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
		if(evaluated_reqfiles >= reqfilenb)
		{
			debug("went through all reqfiles, stopping \n");
			break;
		}

	}
	*current_hash = chosen_hash;
	return chosen_queue;
}

void SRTF(void *clnt)
{
	int SRTF_current_hash=0;
	struct related_list_t *SRTF_current_queue;
	struct request_t *req;



	while(get_current_reqnb() > 0)
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
					process_requests(req, (struct client *)clnt, SRTF_current_hash);
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
	


/**************************************
 *
 * I/O schedulers manager
 *
 **************************************/

// Return 1 if it succeeded
int initialize_scheduler(int index, void *consumer) {
	int ret=1;
	
	
	((struct consumer_t *)consumer)->io_scheduler = find_io_scheduler(index);
	
	if (!(((struct consumer_t *)consumer)->io_scheduler))
		ret = -EINVAL;
	else if (((struct consumer_t *)consumer)->io_scheduler->init)
			ret = ((struct consumer_t *)consumer)->io_scheduler->init();

	return ret;
}

static AGIOS_LIST_HEAD(io_schedulers);

struct io_scheduler_instance_t *find_io_scheduler(int index)
{
	struct io_scheduler_instance_t *ret=NULL;
	int i=0;
	
	agios_list_for_each_entry(ret, &io_schedulers, list)
	{
		if(i == index)
			return ret;
		i++;
	}
	return NULL;
}
		
int register_io_scheduler(struct io_scheduler_instance_t *io_sched)
{

	if (!io_sched->schedule)
		return -1;

	agios_list_add_tail(&io_sched->list, &io_schedulers);
	return 0;
}

void unregister_io_scheduler(int index)
{
	struct io_scheduler_instance_t *sched;

	sched = find_io_scheduler(index);
	if (sched)
		agios_list_del(&sched->list);
}

void register_static_io_schedulers(void)
{
	static struct io_scheduler_instance_t scheds[] = {
		{
			.init = MLF_init,
			.exit = MLF_exit,
			.schedule = &MLF,
			.max_aggreg_size = MAX_AGGREG_SIZE_MLF,
			.sync=0,
			.needs_hashtable=1,
			.name = "MLF",
			.index = 0,
		},
		{
			.init = NULL,
			.exit = NULL,
			.schedule = &timeorder,
			.max_aggreg_size = MAX_AGGREG_SIZE,
			.sync=0,
			.needs_hashtable=0,
			.name = "TO-agg",
			.index = 1,
		},
		{
			.init = NULL,
			.exit = NULL,
			.schedule = &SJF,
			.max_aggreg_size = MAX_AGGREG_SIZE,
			.sync=0,
			.needs_hashtable=1,
			.name = "SJF",
			.index = 2,
		},
		{
			.init = NULL,
			.exit = NULL,
			.schedule = &SRTF,
			.max_aggreg_size = MAX_AGGREG_SIZE,
			.sync=0,
			.needs_hashtable=1,
			.name = "SRTF",
			.index = 3,
		},
		{
			.init = AIOLI_init,
			.exit = NULL,
			.schedule = &AIOLI,
			.max_aggreg_size = MAX_AGGREG_SIZE,
			.sync=1,
			.needs_hashtable=1,
			.name = "aIOLi",
			.index = 4,
		},
		{
			.init = NULL,
			.exit = NULL,
			.schedule = &simple_timeorder,
			.max_aggreg_size = 1,
			.sync=0,
			.needs_hashtable=0,
			.name = "TO",
			.index = 5,
		}
	};
	int i = 0;

	for (i = 0; i < sizeof(scheds) / sizeof(scheds[0]); ++i)
		register_io_scheduler(&scheds[i]);
}

int get_algorithm_from_string(const char *alg)
{
	struct io_scheduler_instance_t *sched=NULL;
	int ret=SJF_SCHEDULER; //default in case we can't find it
	char *this_alg = malloc(sizeof(char)*strlen(alg));

	if(strcmp(alg, "no_AGIOS") == 0)
		strcpy(this_alg, "TO"); //TODO there are some situations where even using TO is bad for performance and the best idea would really be to not use AGIOS at all. We could signal the user about this.
	else
		strcpy(this_alg, alg);
	
	agios_list_for_each_entry(sched, &io_schedulers, list)
	{
		if(strcmp(this_alg, sched->name) == 0)
		{
			ret = sched->index;
			break;
		}
	}
	free(this_alg);
	return ret;
}
//index MUST be an existing scheduling algorithm
char * get_algorithm_name_from_index(int index)
{
	struct io_scheduler_instance_t *sched=NULL;
	char *ret = NULL;
	
	agios_list_for_each_entry(sched, &io_schedulers, list)
	{
		if(index == sched->index)
		{
			ret = sched->name;
			break;
		}
	}
	return ret;
}
