/* File:	iosched.c
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the interface to the scheduling algorithms. 
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
#include "estimate_access_times.h"
#include "request_cache.h"
#include "consumer.h"
#include "agios_config.h"
#include "req_timeline.h"
#include "agios_request.h"

#include "TO.h"
#include "MLF.h"
#include "SRTF.h"
#include "SJF.h"
#include "TW.h"
#include "AIOLI.h"
#include "NOOP.h"
#include "DYN_TREE.h"
#include "ARMED_BANDIT.h"
#include "EXCLUSIVE_TW.h"
#include "PATTERN_MATCHING.h"


/**********************************************************************************************************************/
/*	FOR ALGORITHMS WITH THE SYNCHRONOUS APPROACH	*/
/**********************************************************************************************************************/
//control the synchronous approach:
static short int agios_can_continue=0;
static pthread_mutex_t request_processed_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t request_processed_cond = PTHREAD_COND_INITIALIZER;

void iosched_signal_synchronous(void)
{
	if(current_scheduler->sync)
	{
		pthread_mutex_lock(&request_processed_mutex);
		agios_can_continue=1;
		pthread_cond_signal(&request_processed_cond);
		pthread_mutex_unlock(&request_processed_mutex);
	}
	
}
void iosched_wait_synchronous(void)
{
	if(current_scheduler->sync) //there is no chance is_synchronous will change while we are waiting and then we will receive no signal, since it is the scheduling thread who calls it (it could be called by the add_request part, but only for NOOP, which is not synchronous), and scheduling algorithms only change when this same thread decides to do so
	{
		pthread_mutex_lock(&request_processed_mutex);
		while(!agios_can_continue)
			pthread_cond_wait(&request_processed_cond, &request_processed_mutex);
		agios_can_continue = 0;
		pthread_mutex_unlock(&request_processed_mutex);	
	}
}
/**********************************************************************************************************************/
/*	STATISTICS	*/
/**********************************************************************************************************************/
/*for calculating alpha during execution, which represents the ability to overlap waiting time with processing other requests*/
long int time_spent_waiting=0;
long int waiting_time_overlapped=0;

/**********************************************************************************************************************/
/*	GENERIC HELPING FUNCTIONS USED BY MULTIPLE I/O SCHEDULING ALGORITHMS	*/
/**********************************************************************************************************************/
/*cleans up request_t structures after processing requests*/
void generic_post_process(struct request_t *req)
{
	req->globalinfo->lastaggregation = req->reqnb; 

	if(req->reqnb > 1) //this was an aggregated request
	{
		stats_aggregation(req->globalinfo);
		request_cache_free(req); //we free the virtual request structure, since its parts were included in the dispatch queue as separated requests
	}
}
/*this function is called by the release function, when the library user signaled it finished processing a request. In the case of a virtual request, its requests will be signaled separately, so here we are sure to receive a singel request */
void generic_cleanup(struct request_t *req)
{
	//update the processed requests counter
	req->globalinfo->stats_window.processedreq_nb++;
	req->globalinfo->stats_file.processedreq_nb++;

	//update the data counter
	req->globalinfo->stats_window.processed_req_size += req->io_data.len;
	req->globalinfo->stats_file.processed_req_size += req->io_data.len;

	agios_list_del(&req->related); //remove from the dispatch queue
	request_cache_free(req); //free the memory
}
/* post process function for scheduling algorithms which use waiting times (AIOLI and MLF)*/
void waiting_algorithms_postprocess(struct request_t *req)
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

//this function is used by MLF and by AIOLI. These two schedulers use a sched_factor that increases as request stays in the scheduler queues.
void increment_sched_factor(struct request_t *req)
{
	if(req->sched_factor == 0)
		req->sched_factor = 1;
	else
		req->sched_factor = req->sched_factor << 1;
}

/*used by MLF and AIOLI. After selecting a virtual request for processing, checks if it should be processed right away or if it's better to wait for a while*/
struct request_t *checkSelection(struct request_t *req, struct request_file_t *req_file)
{
	struct request_t *retrn = req;


	/*waiting times are cause by 3 phenomena:*/
	/*1. shift phenomenon. One of the processes issuing requests to this queue is a little delayed, causing a contiguous request to arrive shortly after the other ones*/
	if(req->globalinfo->predictedoff != 0)
	{
		if(req->io_data.offset > req->globalinfo->predictedoff)
		{
			req_file->waiting_time = config_waiting_time;
			stats_shift_phenomenon(req->globalinfo);
			if(config_trace_agios)
			 	agios_trace_shift(config_waiting_time, req->file_id);	
		}
		/*set to 0 to avoid starvation*/
		req->globalinfo->predictedoff = 0;
	} 

	/*2. better aggregation. If we just performed a larger aggregation on this queue, we believe we could do it again*/
	else if((req->io_data.offset > req->globalinfo->lastfinaloff) && (req->globalinfo->lastaggregation > req->reqnb))
	{
		/*set to zero to avoid starvation*/
		req->globalinfo->lastaggregation = 0;
		req_file->waiting_time = config_waiting_time;
		stats_better_aggregation(req->globalinfo);
		if(config_trace_agios)
			agios_trace_better(req->file_id);
	}
	/*3. we predicted that a better aggregation could be done to this request*/
	else if((config_predict_agios_request_aggregation) && ((req_file->waiting_time = agios_predict_should_we_wait(req)) > 0))
	{
		stats_predicted_better_aggregation(req->globalinfo);
		if(config_trace_agios)
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

/*sleeps for timeout ns*/
void agios_wait(int timeout, char *file)
{
#ifndef AGIOS_KERNEL_MODULE
	struct timespec timeout_tspec;
#endif


	if(config_trace_agios)
		agios_trace_wait(timeout,file);
	
	debug("going to sleep for %u\n", timeout);

#ifdef AGIOS_KERNEL_MODULE
	/*TODO see if this approach really works as expected*/
	set_current_state(TASK_INTERRUPTIBLE);
	ndelay(timeout);
	set_current_state(TASK_RUNNING);		
#else

	timeout_tspec.tv_sec = (int) timeout / 1000000000L;
	timeout_tspec.tv_nsec = (int) timeout % 1000000000L;

	nanosleep(&timeout_tspec, NULL);
#endif
}

//used by AIOLI and MLF, the algorithms which employ waiting times. Since we try not to wait (when waiting on one file, we go on processing requests to other files), every time we try to get requests from a file we need to update its waiting time to see if it is still waiting or not
void update_waiting_time_counters(struct request_file_t *req_file, int *smaller_waiting_time, struct request_file_t **swt_file )
{
	long int elapsed;

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

//used by AIOLI and MLF to init the waiting time statistics
void generic_init()
{ //I've removed the initialization of the waiting time statistics from here because we don't want them to be reseted every time we change the scehduling algorithm
}


/**********************************************************************************************************************/
/*	FUNCTIONS TO I/O SCHEDULING ALGORITHMS MANAGEMENT (INITIALIZATION, SETTING, ETC)	*/
/**********************************************************************************************************************/
int current_alg = 0;
struct io_scheduler_instance_t *current_scheduler=NULL;
static AGIOS_LIST_HEAD(io_schedulers); //the list of scheduling algorithms (with their parameters)
static int io_schedulers_nb = 12
static struct io_scheduler_instance_t io_schedulers[] = { //ATTENTION! If you are going to add or remove an I/O scheduler, dont forget to update io_schedulers_nb and the indexes here and in the iosched.h file! 
		{
			.init = MLF_init,
			.exit = MLF_exit,
			.schedule = &MLF,
			.select_algorithm = NULL,
			.max_aggreg_size = MAX_AGGREG_SIZE_MLF,
			.sync=0,
			.needs_hashtable=1,
			.can_be_dynamically_selected=1,
			.is_dynamic = 0,
			.name = "MLF",
			.index = 0,
		},
		{
			.init = NULL,
			.exit = NULL,
			.schedule = &timeorder, // Timeorder with aggregation
			.select_algorithm = NULL,
			.max_aggreg_size = MAX_AGGREG_SIZE,
			.sync=0,
			.needs_hashtable=0,
			.can_be_dynamically_selected=1,
			.is_dynamic = 0,
			.name = "TO-agg",
			.index = 1,
		},
		{
			.init = NULL,
			.exit = NULL,
			.schedule = &SJF,
			.select_algorithm = NULL,
			.max_aggreg_size = MAX_AGGREG_SIZE,
			.sync=0,
			.needs_hashtable=1,
			.can_be_dynamically_selected=1,
			.is_dynamic = 0,
			.name = "SJF",
			.index = 2,
		},
		{
			.init = NULL,
			.exit = NULL,
			.schedule = &SRTF,
			.select_algorithm = NULL,
			.max_aggreg_size = MAX_AGGREG_SIZE,
			.sync=0,
			.needs_hashtable=1,
			.can_be_dynamically_selected=0,
			.is_dynamic = 0,
			.name = "SRTF",
			.index = 3,
		},
		{
			.init = AIOLI_init,
			.exit = NULL,
			.schedule = &AIOLI,
			.select_algorithm = NULL,
			.max_aggreg_size = MAX_AGGREG_SIZE,
			.sync=1,
			.needs_hashtable=1,
			.can_be_dynamically_selected=0,
			.is_dynamic = 0,
			.name = "aIOLi",
			.index = 4,
		},
		{
			.init = NULL,
			.exit = NULL,
			.schedule = &simple_timeorder, // Timeorder
			.select_algorithm = NULL,
			.max_aggreg_size = 1,
			.sync=0,
			.needs_hashtable=0,
			.can_be_dynamically_selected=1,
			.is_dynamic = 0,
			.name = "TO",
			.index = 5,
		},
		{
			.init = NULL,
			.exit = NULL,
			.schedule = &time_window,
			.select_algorithm = NULL,
			.max_aggreg_size = 1,
			.sync=0,
			.needs_hashtable=0, 
			.can_be_dynamically_selected=0,
			.is_dynamic = 0,
			.name = "TW",
			.index = 6,
		},
		{
			.init = NULL,
			.exit = NULL,
			.schedule = &NOOP,
			.select_algorithm = NULL,
			.max_aggreg_size = 1,
			.sync = 0,  //NOOP cannot ever be sync, otherwise it will probably cause deadlock at the user 
			.needs_hashtable= 0,
			.can_be_dynamically_selected=1,
			.is_dynamic = 0,
			.name = "NOOP",
			.index = 7,
		},
		{
			.init = &DYN_TREE_init,
			.exit = NULL,
			.schedule = NULL,
			.select_algorithm = &DYN_TREE_select_next_algorithm,
			.max_aggreg_size = 1,
			.sync = 0,
			.needs_hashtable = 0,
			.can_be_dynamically_selected = 0,
			.is_dynamic = 1,
			.name = "DYN_TREE",
			.index = 8,
		},
		{
			.init = &ARMED_BANDIT_init,
			.exit = &ARMED_BANDIT_exit,
			.schedule = NULL,
			.select_algorithm = &ARMED_BANDIT_select_next_algorithm,
			.max_aggreg_size = 1,
			.sync = 0,
			.needs_hashtable = 0,
			.can_be_dynamically_selected = 0,
			.is_dynamic = 1,
			.name = "ARMED_BANDIT",
			.index = 9,
		},
		{
			.init = &TWINS_init,
			.exit = &TWINS_exit,
			.schedule = &TWINS,
			.select_algorithm = NULL,
			.max_aggreg_size = 1,
			.sync = 0, 
			.needs_hashtable = 0, 
			.can_be_dynamically_selected = 0, //The functions that implement the migration between different scheduling algorithms were not adapted for this algorithm, so it should never be used with a dynamic algorithm until we fix that.  
			.is_dynamic = 0,
			.name = "TWINS",
			.index = 10,
		},
		{
			.init = &PATTERN_MATCHING_init,
			.exit = &PATTERN_MATCHING_exit,
			.schedule = NULL,
			.select_algorithm = &PATTERN_MATCHING_select_next_algorithm,
			.max_aggreg_size = 1,
			.sync = 0,
			.needs_hashtable = 0,
			.can_be_dynamically_selected = 0,
			.is_dynamic = 1,
			.name = "PATTERN_MATCHING",
			.index = 11,
		}
	};

//change the current scheduling algorithm and update local parameters
//here we assume the scheduling thread is NOT running, so it won't mess with the structures
// it will acquire the lock to all data structures, must call unlock afterwards
void change_selected_alg(int new_alg)
{
	int previous_alg;
	struct io_scheduler_instance_t *previous_scheduler; 

	//TODO handle prediction thread

	PRINT_FUNCTION_NAME;

	//lock all data structures so no one is adding or releasing requests while we migrate
	lock_all_data_structures();

	if(current_alg != new_alg)
	{
		//change scheduling algorithm
		previous_scheduler = current_scheduler;
		previous_alg = current_alg;
		current_scheduler = initialize_scheduler(new_alg);
		current_alg = new_alg;

		//do we need to migrate data structure?
		//first situation: both use hashtable
		if(current_scheduler->needs_hashtable && previous_scheduler->needs_hashtable)
		{
			//the only problem here is if we decreased the maximum aggregation
			//For now we chose to do nothing. If we no longer tolerate aggregations of a certain size, we are not spliting already performed aggregations since this would not benefit us at all. We could rethink that at some point
		}
		//second situation: from hashtable to timeline
		else if (previous_scheduler->needs_hashtable && (!current_scheduler->needs_hashtable))
		{
#ifdef AGIOS_DEBUG
			print_hashtable();
#endif
			migrate_from_hashtable_to_timeline();
#ifdef AGIOS_DEBUG
			print_timeline();
#endif
		}
		//third situation: from timeline to hashtable
		else if ((!previous_scheduler->needs_hashtable) && current_scheduler->needs_hashtable)
		{
#ifdef AGIOS_DEBUG
			print_timeline();
#endif
			migrate_from_timeline_to_hashtable();
#ifdef AGIOS_DEBUG
			print_hashtable();
#endif
		}	
		//fourth situation: both algorithms use timeline
		else
		{
			//now it depends on the algorithms. 
			//if we are changing to NOOP, it does not matter because it does not really use the data structure
			//if we are changing from or to TIME_WINDOW, we need to reorder the list
			//if we are changing to the timeorder with aggregation, we need to reorder the list
			if((current_alg != NOOP_SCHEDULER) && ((previous_alg == TIME_WINDOW_SCHEDULER) || (current_alg == TIME_WINDOW_SCHEDULER) || (current_alg == TIMEORDER_SCHEDULER)))
			{
//				reorder_timeline(); 
			}
		}
	}
}


//finds and returns the current scheduler indicated by index. If this scheduler needs an initialization function, calls it
struct io_scheduler_instance_t *initialize_scheduler(int index) 
{
	if((index >= io_schedulers_nb) || (index < 0))
		return NULL;
	if(io_schedulers[index].init)
	{
		if(io_schedulers[index].init() != 0)
			return NULL;
	}
	PRINT_FUNCTION_EXIT;
	return &(io_schedulers[index]);
}

struct io_scheduler_instance_t *find_io_scheduler(int index)
{
	if((index >= io_schedulers_nb) || (index < 0))
		return NULL;
	return &(io_schedulers[index]);
}
//this is like get_algorithm_name_from_index, except that the parameter string could be "no_AGIOS", and in that case we translate it to NOOP 		
int get_algorithm_from_string(const char *alg)
{
	struct io_scheduler_instance_t *sched=NULL;
	int ret=SJF_SCHEDULER; //default in case we can't find it
	char *this_alg; 
	short int need_free=0;
	int i;

	if(strcmp(alg, "no_AGIOS") == 0)
	{	
		this_alg = malloc(sizeof(char)*5);
		strcpy(this_alg, "NOOP"); 
		need_free=1;
	}
	else
		this_alg = alg;
	
	for(i=0; i < io_schedulers_nb; i++)
	{
		if(strcmp(this_alg, io_schedulers[i].name) == 0)
		{
			ret = i;
			break;
		}
	}
	return ret;
}
char * get_algorithm_name_from_index(int index)
{
	if((index >= io_schedulers_nb) || (index < 0))
		return NULL;
	return io_schedulers[index].name;
}
void enable_TW(void)
{
	io_schedulers[TIME_WINDOW_SCHEDULER].can_be_dynamically_selected = 1;
}
