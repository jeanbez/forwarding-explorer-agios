/* File:	consumer.c
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 * Collaborators:
 *		Jean Luca Bez <jlbez (at) inf.ufrgs.br>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the code for the AGIOS thread. It sleeps when there are no 
 *		requests, and calls the scheduler otherwise.
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


#include "agios.h"
#include "mylist.h"
#include "request_cache.h"
#include "iosched.h"		
#include "consumer.h"
#include "trace.h"
#include "common_functions.h"
#include "agios_config.h"
#include "predict.h"
#include "req_hashtable.h"
#include "req_timeline.h"
#include "proc.h"
#include "performance.h"
#include "agios_request.h"

#ifndef AGIOS_KERNEL_MODULE
#include <pthread.h>
#else
#include <linux/kthread.h>
#endif

/***********************************************************************************************************
 * PARAMETERS AND FUNCTIONS TO CONTROL THE I/O SCHEDULING THREAD *
 ***********************************************************************************************************/
#ifdef AGIOS_KERNEL_MODULE
struct task_struct	*consumer_task;
#else
int consumer_task;
#endif
struct client *client;
//so we can let the AGIOS thread know we have new requests (so it can schedule them)
#ifdef AGIOS_KERNEL
static struct completion request_added;
#else
static pthread_cond_t request_added_cond;
static pthread_mutex_t request_added_mutex;
#endif
#ifdef AGIOS_KERNEL_MODULE
static struct completion exited;
void consumer_wait_completion(void)
{
	wait_for_completion(&exited);
}
#endif
//signal new requests
void consumer_signal_new_reqs(void)
{
#ifdef AGIOS_KERNEL_MODULE
	complete(&request_added);
#else
	agios_mutex_lock(&request_added_mutex);
	agios_cond_signal(&request_added_cond);
	agios_mutex_unlock(&request_added_mutex);
#endif
	
}
#ifdef AGIOS_KERNEL_MODULE
void consumer_init(struct client *clnt_value, struct task_struct *task_value)
#else
void consumer_init(struct client *clnt_value, int task_value)
#endif
{
#ifdef AGIOS_KERNEL_MODULE
	init_completion(&exited);
	init_completion(&request_added);
#else
	agios_cond_init(&request_added_cond);
	agios_mutex_init(&request_added_mutex);
#endif
	consumer_task = task_value;
	client = clnt_value;	
	agios_processed_reqnb=0;
}
#ifndef AGIOS_KERNEL_MODULE
int agios_thread_stop = 0;
pthread_mutex_t agios_thread_stop_mutex = PTHREAD_MUTEX_INITIALIZER;

void stop_the_agios_thread(void)
{
	agios_mutex_lock(&agios_thread_stop_mutex);
	agios_thread_stop = 1;
	agios_mutex_unlock(&agios_thread_stop_mutex);
}
#endif
int agios_thread_should_stop(void)
{
#ifdef AGIOS_KERNEL_MODULE
	return kthread_should_stop();
#else
	int ret;
	
	agios_mutex_lock(&agios_thread_stop_mutex);
	ret = agios_thread_stop;
	agios_mutex_unlock(&agios_thread_stop_mutex);

	return ret;
#endif
}



/***********************************************************************************************************
 * CURRENT SCHEDULING ALGORITHM AND ITS PARAMETERS 	   *
 ***********************************************************************************************************/
static struct io_scheduler_instance_t *dynamic_scheduler=NULL;
static struct timespec last_algorithm_update; //the time at the last time we've selected an algorithm
struct io_scheduler_instance_t *consumer_get_current_scheduler(void)
{
	return current_scheduler;
}

/***********************************************************************************************************
 * REFRESH OF PREDICTION MODULE'S ALPHA FACTOR 	   *
 ***********************************************************************************************************/
//TODO this does not work, redo

/***********************************************************************************************************
 * REQUESTS PROCESSING (CALLED BY THE I/O SCHEDULERS THROUGH THE AGIOS THREAD)	   *
 ***********************************************************************************************************/
//unlocks the mutex protecting the data structure where the request is being held.
//if it is the hashtable, then hash gives the line of the table (because we have one mutex per line)
//if hash= -1, we are using the timeline (a simple list with only one mutex)
inline void unlock_structure_mutex(int hash)
{
	if(hash >= 0) 
		hashtable_unlock(hash);
	else
		timeline_unlock();
}
//locks the mutex protecting the data structure where the request is being held.
//if it is the hashtable, then hash gives the line of the table (because we have one mutex per line)
//if hash= -1, we are using the timeline (a simple list with only one mutex)
inline void lock_structure_mutex(int hash)
{
	if(hash >= 0) 
		hashtable_lock(hash);
	else
		timeline_lock();
}

inline void put_this_request_in_dispatch(struct request_t *req, unsigned long int this_time, struct agios_list_head *dispatch)
{
	agios_list_add_tail(&req->related, dispatch);
	req->dispatch_timestamp = this_time;
	debug("request - size %lu, offset %lu, file %s - going back to the file system", req->io_data.len, req->io_data.offset, req->file_id);
	req->globalinfo->current_size -= req->io_data.len; //when we aggregate overlapping requests, we don't adjust the related list current_size, since it is simply the sum of all requests sizes. For this reason, we have to subtract all requests from it individually when processing a virtual request.
	req->globalinfo->req_file->timeline_reqnb--;
	
	
}

//must hold relevant data structure mutex (it will release and then get it again tough)
//it will return 1 if some refresh period has expired (it is time to recalculate the alpha factor and redo all predictions, or it is time to change the scheduling algorithm), 0 otherwise
short int process_requests(struct request_t *head_req, struct client *clnt, int hash)
{
	struct request_t *req, *aux_req=NULL;
	short int update_time=0;	
	struct timespec now;
	unsigned long int this_time;
	
	if(!head_req)
		return 0; /*whaaaat?*/

	PRINT_FUNCTION_NAME;
	agios_gettime(&now);


	if(config_trace_agios)
		agios_trace_process_requests(head_req);

	if((clnt->process_requests) && (head_req->reqnb > 1)) //if the user has a function capable of processing a list of requests at once and this is an aggregated request
	{
		this_time = get_timespec2llu(now);
#ifdef ORANGEFS_AGIOS
		int64_t *reqs = (int64_t *)malloc(sizeof(int64_t)*(head_req->reqnb+1));
#else
		void **reqs = (void **)malloc(sizeof(void *)*(head_req->reqnb+1));
#endif
		int reqs_index=0;
		//we give to the user an array with the requests' data (the field used by the user, provided in agios_add_request)
		agios_list_for_each_entry(req, &head_req->reqs_list, related)
		{
			if(aux_req) //we can't just mess with req because the for won't be able to find the next requests after we've modified this one's pointers
			{
				put_this_request_in_dispatch(aux_req, this_time, &head_req->globalinfo->dispatch);
				reqs[reqs_index]=aux_req->data;
				reqs_index++;
			}
			aux_req = req;
		}
		if(aux_req)
		{
			put_this_request_in_dispatch(aux_req, this_time, &head_req->globalinfo->dispatch);
			reqs[reqs_index]=aux_req->data;
			reqs_index++;
		}
		//update requests and files counters
		if(head_req->globalinfo->req_file->timeline_reqnb == 0)
			dec_current_reqfilenb();
		dec_many_current_reqnb(hash, head_req->reqnb);
		//process
		clnt->process_requests(reqs, head_req->reqnb);
		//we no longer need this array
		free(reqs);
	}
	else{
		if(head_req->reqnb == 1)
		{
			put_this_request_in_dispatch(head_req, get_timespec2llu(now), &head_req->globalinfo->dispatch);
			if(head_req->globalinfo->req_file->timeline_reqnb == 0)
				dec_current_reqfilenb();
			dec_current_reqnb(hash);
			clnt->process_request(head_req->data);
		}
		else
		{
			//it is a virtual request but the user did not provide a function to process multiple requests at once, so we'll simply give them sequentially 
			this_time = get_timespec2llu(now);
			agios_list_for_each_entry(req, &head_req->reqs_list, related)
			{	
				if(aux_req)
				{
					put_this_request_in_dispatch(aux_req, this_time, &head_req->globalinfo->dispatch);
					clnt->process_request(aux_req->data);
				}
				aux_req = req;
			}
			if(aux_req)
			{
				put_this_request_in_dispatch(aux_req, this_time, &head_req->globalinfo->dispatch);
				clnt->process_request(aux_req->data);
			}

			if(head_req->globalinfo->req_file->timeline_reqnb == 0)
				dec_current_reqfilenb();
			dec_many_current_reqnb(hash, head_req->reqnb);
		}
	}

//	if((config_predict_agios_recalculate_alpha_period >= 0) && (agios_processed_reqnb  >= config_predict_agios_recalculate_alpha_period))
//		update_time=1;
//	debug("scheduler is dynamic? %d, algorithm_selection_period = %lu, processed_reqnb = %lu, last_selection_processed_renb = %lu, algorithm_selection_reqnumber = %lu", dynamic_scheduler->is_dynamic, algorithm_selection_period, processed_reqnb, last_selection_processed_reqnb, algorithm_selection_reqnumber);
	if((dynamic_scheduler->is_dynamic) && (config_agios_select_algorithm_period >= 0) && (agios_processed_reqnb >= config_agios_select_algorithm_min_reqnumber))
	{
		if(get_nanoelapsed(last_algorithm_update) >= config_agios_select_algorithm_period)
			update_time=1;
	}


	debug("current status. hashtable[%d] has %d requests, there are %d requests in the scheduler to %d files.", hash, hashlist_reqcounter[hash], current_reqnb, current_reqfilenb); //attention: it could be outdated info since we are not using the lock

	//if the current scheduling algorithm follows the synchronous approach, we need to wait until the request was processed
	if((current_scheduler->sync) && (!update_time))
	{
		//we need to release the hashtable/timeline mutex while waiting because otherwise other thread will not be able to acquire it in the release_request function and signal us
		unlock_structure_mutex(hash); 
		iosched_wait_synchronous();
		lock_structure_mutex(hash);	
	}	

	PRINT_FUNCTION_EXIT;
	return update_time;
}

void check_update_time()
{
	int next_alg = current_alg;

	//is it time to change the scheduling algorithm?
	//TODO it is possible that at this moment the prediction thread is reading traces and making predictions, i.e., accessing the hashtable. This could lead to awful problems. If we want to test with recalculate_alpha_period, we need to take care of that!
	if((dynamic_scheduler->is_dynamic) && (config_agios_select_algorithm_period >= 0) && (agios_processed_reqnb >= config_agios_select_algorithm_min_reqnumber) && (get_nanoelapsed(last_algorithm_update) >= config_agios_select_algorithm_period))
	{
		//make a decision on the next scheduling algorithm
		next_alg = dynamic_scheduler->select_algorithm();
		//change it
		change_selected_alg(next_alg);
		proc_set_new_algorithm(current_alg);
		performance_set_new_algorithm(current_alg);
		reset_stats_window(); //reset all stats so they will not affect the next selection
		unlock_all_data_structures();
		agios_gettime(&last_algorithm_update); 
		debug("We've changed the scheduling algorithm to %s", current_scheduler->name);

	}
	//is it time to recalculate the alpha factor and review all predicted requests aggregations?
	//we check for this AFTER the change of scheduling algorithm (despite the fact that making it before would potentially allow for new information to be considered in the algorithm selection) because the refresh_predictions() only signals the prediction thread, that will then start reading traces, which may take a long time and access the hashtable. This is not to be made possible during scheduling algorithm migration. We could wait for the prediction thread to finish its operation, but this could mean keeping AGIOS frozen while making predictions, which would impair performance 
//	if((config_predict_agios_recalculate_alpha_period >= 0) && (agios_processed_reqnb  >= config_predict_agios_recalculate_alpha_period))
//	{
//		refresh_predictions();
//	}
}
/***********************************************************************************************************
 * SCHEDULING THREAD
 ***********************************************************************************************************/
#ifdef AGIOS_KERNEL_MODULE
int agios_thread(void *arg)
#else
void * agios_thread(void *arg)
#endif
{
#ifndef AGIOS_KERNEL_MODULE
	struct timespec timeout_tspec;
#endif
	PRINT_FUNCTION_NAME;

	//let's find out which I/O scheduling algorithm we need to use
	dynamic_scheduler = initialize_scheduler(config_agios_default_algorithm);
	if(!dynamic_scheduler->is_dynamic) //we are NOT using a dynamic scheduling algorithm
	{
		current_alg = config_agios_default_algorithm;
		current_scheduler = dynamic_scheduler;
	}
	else //we are using a dynamic scheduler
	{
		//with which algorithm should we start?
		current_alg = config_agios_starting_algorithm; //if we are using DYN_TREE and it selected a scheduling algorithm at its initialization phase, the parameter was updated and we can access it here. If not, we will use the provided starting algorithm
		current_scheduler = initialize_scheduler(current_alg);
		if(config_agios_select_algorithm_period > 0)
		{
			agios_gettime(&last_algorithm_update);			
		}
	}
	proc_set_new_algorithm(current_alg);
	performance_set_new_algorithm(current_alg);
	debug("selected algorithm: %s", current_scheduler->name);
	//since the current algorithm is decided, we can allow requests to be included
	unlock_all_data_structures();
	


	//execution loop, it only stops when we close the library
	do {
		if(get_current_reqnb() == 0) //here we use the mutex to access the variable current_reqnb because we don't want to risk getting an outdated value and then sleeping for nothing
		{
			/* there are no requests to be processed, so we just have to wait until a new request arrives. 
			 * We use a timeout to avoid a situation where we missed the signal and will sleep forever, and
                         * also because we have to check once in a while to see if we should end the execution.
			 */
#ifdef AGIOS_KERNEL_MODULE
			wait_for_completion_timeout(&request_added, config_waiting_time);
#else
			/*fill the timeout structure*/
			timeout_tspec.tv_sec =  config_waiting_time / 1000000000L;
			timeout_tspec.tv_nsec = config_waiting_time % 1000000000L;
			
 			pthread_mutex_lock(&request_added_mutex);
			pthread_cond_timedwait(&request_added_cond, &request_added_mutex, &timeout_tspec);
			pthread_mutex_unlock(&request_added_mutex);
#endif
		}
	
		if(!agios_thread_should_stop()) 
		{
			check_update_time();

			if(current_reqnb > 0) //attention: it could be outdated info since we are not using the lock
			{
				current_scheduler->schedule(client);
			}
	
			check_update_time();
		}
        } while (!agios_thread_should_stop());

	consumer_task = 0;
#ifdef AGIOS_KERNEL_MODULE
	complete(&exited);
#endif
	return 0;
}
	


