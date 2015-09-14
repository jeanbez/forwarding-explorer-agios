/* File:	iosched.c
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 * Collaborators:
 *		Jean Luca Bez <jlbez (at) inf.ufrgs.br>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the interface to the scheduling algorithms. 
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

#include "TO.h"
#include "MLF.h"
#include "SRTF.h"
#include "SJF.h"
#include "TW.h"
#include "AIOLI.h"
#include "NOOP.h"


static AGIOS_LIST_HEAD(io_schedulers); //the list of scheduling algorithms (with their parameters)

/**********************************************************************************************************************/
/*	LOCAL COPIES OF CONFIGURATION FILE PARAMETERS	*/
/**********************************************************************************************************************/
//these parameters are obtained from the configuration file at the beginning of the execution. We keep copies for performance reasons
static short int predict_request_aggregation=0;
static short int trace=0;
inline void set_iosched_predict_request_aggregation(short int value)
{
	predict_request_aggregation=value;
}
inline void set_iosched_trace(short int value)
{
	trace = value;
}

/**********************************************************************************************************************/
/*	STATISTICS	*/
/**********************************************************************************************************************/
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
/**********************************************************************************************************************/
/*	GENERIC HELPING FUNCTIONS USED BY MULTIPLE I/O SCHEDULING ALGORITHMS	*/
/**********************************************************************************************************************/
/*cleans up request_t structures after processing requests*/
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
inline void increment_sched_factor(struct request_t *req)
{
	if(req->sched_factor == 0)
		req->sched_factor = 1;
	else
		req->sched_factor = req->sched_factor << 1;
}

/*used by MLF and AIOLI. After selecting a virtual request for processing, checks if it should be processed right away or if it's better to wait for a while*/
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
			if(trace)
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
		if(trace)
			agios_trace_better(req->file_id);
	}
	/*3. we predicted that a better aggregation could be done to this request*/
	else if((predict_request_aggregation) && ((req_file->waiting_time = agios_predict_should_we_wait(req)) > 0))
	{
		stats_predicted_better_aggregation(req->globalinfo);
		if(trace)
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
void agios_wait(unsigned long long int  timeout, char *file)
{
#ifndef AGIOS_KERNEL_MODULE
	struct timespec timeout_tspec;
#endif


	if(trace)
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

//used by AIOLI and MLF, the algorithms which employ waiting times. Since we try not to wait (when waiting on one file, we go on processing requests to other files), every time we try to get requests from a file we need to update its waiting time to see if it is still waiting or not
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

//used by AIOLI and MLF to init the waiting time statistics
void generic_init()
{
	time_spent_waiting=0;
	waiting_time_overlapped = 0;
}


/**********************************************************************************************************************/
/*	FUNCTIONS TO I/O SCHEDULING ALGORITHMS MANAGEMENT (INITIALIZATION, SETTING, ETC)	*/
/**********************************************************************************************************************/
//updates the consumer structure with the current scheduler indicated by index. If this scheduler needs an initialization function, calls it
//TODO if we call MLF_init or AIOLI_init (generic_init) they will set waiting time statistics to 0. When dynamically changing scheduling algorithms, do we really want this? 
int initialize_scheduler(int index, void *consumer) {
	int ret=1;
	
	
	((struct consumer_t *)consumer)->io_scheduler = find_io_scheduler(index);
	
	if (!(((struct consumer_t *)consumer)->io_scheduler))
		ret = -EINVAL;
	else if (((struct consumer_t *)consumer)->io_scheduler->init)
			ret = ((struct consumer_t *)consumer)->io_scheduler->init();

	return ret;
}

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
			.can_be_dynamically_selected=1,
			.name = "MLF",
			.index = 0,
		},
		{
			.init = NULL,
			.exit = NULL,
			.schedule = &timeorder, // Timeorder with aggregation
			.max_aggreg_size = MAX_AGGREG_SIZE,
			.sync=0,
			.needs_hashtable=0,
			.can_be_dynamically_selected=1,
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
			.can_be_dynamically_selected=1,
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
			.can_be_dynamically_selected=0,
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
			.can_be_dynamically_selected=1,
			.name = "aIOLi",
			.index = 4,
		},
		{
			.init = NULL,
			.exit = NULL,
			.schedule = &simple_timeorder, // Timeorder
			.max_aggreg_size = 1,
			.sync=0,
			.needs_hashtable=0,
			.can_be_dynamically_selected=1,
			.name = "TO",
			.index = 5,
		},
		{
			.init = NULL,
			.exit = NULL,
			.schedule = &time_window,
			.max_aggreg_size = 1,
			.sync=0,
			.needs_hashtable=0, // Não ocupa o hash table, coloca as requisições em uma lista
			.can_be_dynamically_selected=0,
			.name = "TW",
			.index = 6,
		},
		{
			.init = NULL,
			.exit = NULL,
			.schedule = &NOOP,
			.max_aggreg_size = 1,
			.sync = 0,
			.needs_hashtable= 0,
			.can_be_dynamically_selected=1,
			.name = "NOOP"
			.index = 7,
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
		strcpy(this_alg, "NOOP"); 
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
