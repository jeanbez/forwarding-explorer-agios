/* File:	predict.c
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It implements the Prediction Module. It is responsible for reading traces,
 *		handling predicted requests, predicting request aggregations, detecting
 *		access pattern's aspects (spatiality and request size), and scheduling
 *		algorithm selection through a decision tree.
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
#ifndef AGIOS_KERNEL_MODULE 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "predict.h"
#include "mylist.h"
#include "request_cache.h"
#include "iosched.h"
#include "trace.h"
#include "estimate_access_times.h"
#include "hash.h"
#include "common_functions.h"
#include "proc.h"
#include "access_pattern_detection_tree.h"
#include "scheduling_algorithm_selection_tree.h"
#include "agios_config.h"

static int prediction_thr_stop = 0; /*for being notified about finishing the scheduler's execution*/
static pthread_mutex_t prediction_thr_stop_mutex = PTHREAD_MUTEX_INITIALIZER;

static AGIOS_LIST_HEAD(predict_timeline); /*for storing predicted future requests*/

static pthread_t prediction_thread; /*prediction thread */

/*for synchronizing with the scheduling thread*/
static pthread_cond_t prediction_thr_refresh_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t prediction_thr_refresh_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t initialized_prediction_thr_cond = PTHREAD_COND_INITIALIZER;
int initialized_predict=0;

static long double prediction_alpha=0.0; /*represents the ability to overlap waiting times with processing other requests. Used for predicting aggregations*/

static short int new_trace_file=0; /*flag for refreshing predictions*/

static int tracefile_counter=0; /*number of tracefiles considered*/
static int simple_tracefile_counter=0;

static int current_predicted_reqfilenb=0; /*to how many files do we have predicted requests?*/
static pthread_mutex_t current_predicted_reqfilenb_mutex=PTHREAD_MUTEX_INITIALIZER;

unsigned long long int predict_init_time=0; /*the time (in ns) it took to read all traces*/


/*predicted access pattern - updated every time the prediction module is "refreshed" with new trace information*/
/*this access pattern considers information from predictions only, not from scheduler's recent accesses*/
static short int predicted_ap_spatiality=-1;
static short int predicted_ap_reqsize=-1;
static int predicted_ap_operation=-1;
static long long predicted_ap_server_reqsize=-1;
static long int predicted_ap_fileno = -1;


//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//FUNCTIONS RELATED TO THE PREDICTION MODULE'S LIFE
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/*wait until the prediction module finished reading trace files 
 *called by other thread (not prediction thread*/
void predict_init_wait()
{
	agios_mutex_lock(&prediction_thr_refresh_mutex);
	if(initialized_predict == 0)
		agios_cond_wait(&initialized_prediction_thr_cond, &prediction_thr_refresh_mutex);
	agios_mutex_unlock(&prediction_thr_refresh_mutex);
}
/*signals that the prediction module finished reading trace files
 *called by the prediction thread to wake up other threads that may be waiting*/
//TODO instead of waiting until we can start using our scheduler, we could start with the default scheduler and then change when this signal comes
void signal_predict_init()
{
	initialized_predict=1;
	agios_cond_broadcast(&initialized_prediction_thr_cond);
}
unsigned long long int get_predict_init_time()
{
	return predict_init_time;
}
/*used by the main thread to order this prediction thread to stop*/
void stop_prediction_thr(void)
{
#ifdef AGIOS_KERNEL_MODULE
	kthread_stop(prediction_thread);
#else
	pthread_mutex_lock(&prediction_thr_stop_mutex);
	prediction_thr_stop = 1;
	pthread_mutex_unlock(&prediction_thr_stop_mutex);
#endif
	prediction_notify_new_trace_file(); //notify the thread, otherwise it will keep waiting (it has to be awake to see that we want it to stop)
}
/*used by this prediction thread to check if it is time to stop*/
int prediction_thr_should_stop()
{
#ifdef AGIOS_KERNEL_MODULE
	return kthread_should_stop();
#else
	int ret;
	
	agios_mutex_lock(&prediction_thr_stop_mutex);
	ret = prediction_thr_stop;
	agios_mutex_unlock(&prediction_thr_stop_mutex);

	return ret;
#endif
}
void lock_prediction_thr_refresh_mutex()
{
	agios_mutex_lock(&prediction_thr_refresh_mutex);
}
void unlock_prediction_thr_refresh_mutex()
{
	agios_mutex_unlock(&prediction_thr_refresh_mutex);
}
/* must NOT be called by the prediction_thr, but used by the scheduler thread in order to request the prediction thr to 
 * recalculate the prediction_alpha and re-predict all aggregations*/
void refresh_predictions()
{
	agios_mutex_lock(&prediction_thr_refresh_mutex);	
	agios_cond_signal(&prediction_thr_refresh_cond);
	agios_mutex_unlock(&prediction_thr_refresh_mutex);
}
/* must NOT be called by the prediction_thr, but used by the scheduler thread in order to announce a new trace file (that 
 * must have been closed before). The prediction alpha will be recalculated and all the aggregations will be re-predicted*/
void prediction_notify_new_trace_file()
{
	agios_mutex_lock(&prediction_thr_refresh_mutex);	
	new_trace_file=1;
	agios_cond_signal(&prediction_thr_refresh_cond);
	agios_mutex_unlock(&prediction_thr_refresh_mutex);
}
/*how many tracefiles were we able to read?*/
int get_predict_tracefile_counter()
{
	return tracefile_counter;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//FUNCTIONS RELATED TO ADDING REQUESTS
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/*predicted_reqfilenb is protected because it can be accesed by other threads to decide about waiting times, for instance*/
void inc_current_predicted_reqfilenb()
{
	agios_mutex_lock(&current_predicted_reqfilenb_mutex);
	current_predicted_reqfilenb++;
	agios_mutex_unlock(&current_predicted_reqfilenb_mutex);
}
int get_current_predicted_reqfilenb()
{
	int ret;
	agios_mutex_lock(&current_predicted_reqfilenb_mutex);
	ret = current_predicted_reqfilenb;
	agios_mutex_unlock(&current_predicted_reqfilenb_mutex);
	return ret;
}
/*build a new request structure and initialize some fields*/
inline struct request_t *predicted_request_constructor(char *file_id, int type, long long offset, long len, unsigned long long int predicted_time)
{
	struct request_t *new = request_constructor(file_id, type, offset, len, 0, predicted_time, RS_PREDICTED);
	
	new->predicted_aggregation_size = 1;
	new->predicted_aggregation_start = NULL;
	new->first_agg_time = new->last_agg_time = predicted_time;

	return new;
}
/*checks if time1 is close enough to time2 considering an error factor of prediction_time_error
 *this is used to deciding if predictions are for the same request*/
inline int request_time_is_close_enough(unsigned long long int time1, unsigned long long int time2)
{
	if(time1 >= (time2 - (time2 * config_get_prediction_time_error())/100))
	{
		if(time1 <= (time2 + (time2 * config_get_prediction_time_error())/100))
			return 1;
		else
			return 0;
	}
	else
		return 0;
}
/*add a new predicted request to the prediction module's timeline*/
//TODO should we have a mutex for predict_timeline?
void predict_timeline_add_req(struct request_t *req) 
{
	agios_list_add_tail(&req->timeline, &predict_timeline);
}
/*searches for the already existing predicted request inside a related list. MUST hold hashtable lock*/
int check_existing_prediction_onlist(struct agios_list_head *related_list, long long offset, long len, unsigned long long int predicted_time, struct request_t **same_req)
{
	struct request_t *tmp;
	int request_found = 0;

	agios_list_for_each_entry(tmp, related_list, related)
	{
		/*we consider it the same request if it is to the same file, has the same type, for the same offset, with the same datasize and its arrival time is close enough*/
		if((tmp->io_data.offset == offset) && (tmp->io_data.len == len) && (request_time_is_close_enough(predicted_time, (tmp->jiffies_64/tmp->reqnb))))
		{
			request_found = 1;
			*same_req = tmp;
			break;
		}
		/*since the list is sorted by offset, there is no point in searching through all of the requests*/
		else if(tmp->io_data.offset > offset)
			break;
	}
	return request_found;
}
/*searches for the already existing predicted request. Returns it in same_req and its hash value to access in the hashtable*/
int check_existing_prediction(char *file_id, int type, long long offset, long len, unsigned long long int predicted_time, struct request_t **same_req, unsigned long *hash)
{
	unsigned long hash_val;
	struct agios_list_head *hash_list;
	struct request_file_t *req_file;
	int request_file_found=0;
	int ret=0;
	struct agios_list_head *related_list;


	hash_val = AGIOS_HASH_STR(file_id) % AGIOS_HASH_ENTRIES;
	
	hash_list = hashtable_lock(hash_val);
	
	if(!agios_list_empty(hash_list))
	{

		agios_list_for_each_entry(req_file, hash_list, hashlist)
		{
			if(strcmp(req_file->file_id, file_id) >= 0)
			{
				request_file_found=1;
				break;
			}
		}

		if(((request_file_found) && strcmp(req_file->file_id, file_id) == 0))
		{
	
			if(type == RT_READ)
				related_list = &req_file->predicted_reads.list;
			else
				related_list = &req_file->predicted_writes.list;

			if(!agios_list_empty(related_list))
			{
				if(check_existing_prediction_onlist(related_list, offset, len, predicted_time, same_req))
				{
					*hash = hash_val;
					ret = 1;
				}
			}
		}
	}


	hashtable_unlock(hash_val);

	return ret;
}
/*used by the prediction thread, adds a predicted request to the data structures*/
int add_prediction(char *file_id, int type, long long offset, long len, unsigned long long int predicted_time)
{
	unsigned long hash;
	struct request_t *req;
	unsigned long long int new_predicted_time;
	struct request_file_t *req_file;
	struct agios_list_head *hash_list;

	/*we need to update the request's arrival time according to its file*/
	hash = AGIOS_HASH_STR(file_id) % AGIOS_HASH_ENTRIES;
	hash_list = hashtable_lock(hash);
	req_file = find_req_file(hash_list, file_id, RS_PREDICTED);
	if(req_file->first_request_predicted_time == -1)
	{
		req_file->first_request_predicted_time = predicted_time;
		new_predicted_time = 0;
	}
	else
		new_predicted_time = predicted_time -  req_file->first_request_predicted_time;
	hashtable_unlock(hash);
	
	

	/*see if we already have this prediction*/
	if(check_existing_prediction(file_id, type, offset, len, new_predicted_time, &req, &hash))
	{
		/*adjust the predicted time*/
		hashtable_lock(hash);
		req->jiffies_64 += new_predicted_time; //we used to take the average at this point, but this would give more importance to the most recently read predictions. 
		req->reqnb++;
		update_average_distance(req->globalinfo, offset, len);
		hashtable_unlock(hash);
		return 0;
	}
	req = predicted_request_constructor(file_id, type, offset, len, new_predicted_time);

	if(req)
	{
		hash = hashtable_add_req(req);
		proc_stats_newreq(req);  //update statistics
		update_average_distance(req->globalinfo, offset, len);
		hashtable_unlock(hash);
		predict_timeline_add_req(req);

		return 0;
	}
	return -ENOMEM;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//FUNCTIONS USED BY THE SCHEDULING THREAD TO LINK REQUESTS TO THEIR PREDICTED VERSIONS AND TO DECIDE ABOUT WAITING TIMES AND SCHEDULING ALGORITHMS
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/*called by other thread (not by the prediction thread) when there is a new request in the scheduler (a real one) 
 *find the corresponding predicted one and links them*/
void prediction_newreq(struct request_t *req)
{
	struct request_t *tmp;
	struct agios_list_head *predicted_list;

#ifdef AGIOS_DEBUG
	agios_print("looking for the request %s %d %lld %ld...", req->file_id, req->type, req->io_data.offset, req->io_data.len);
#endif

	if(req->type == RT_READ)
		predicted_list = &req->globalinfo->req_file->predicted_reads.list;
	else
		predicted_list = &req->globalinfo->req_file->predicted_writes.list;
	/*find the same request among the predicted ones*/
	if(check_existing_prediction_onlist(predicted_list, req->io_data.offset, req->io_data.len, req->jiffies_64 - req->globalinfo->req_file->first_request_time, &tmp))
	{
		/*link the requests*/	
		req->mirror = tmp;
	}
}

/*see if the performed aggregation is as good as predicted. If it is not,  decides how much time we should wait for new requests*/
unsigned long long int agios_predict_should_we_wait(struct request_t *req)
{
	unsigned long long int waiting_time = 0;
	struct request_t *predicted_head=NULL;
	struct request_t *tmp;
	unsigned long long int max, pmax;

	
	/*TODO maybe remove the already_waited condition. In order to avoid starvation, we could make it not wait if all the requests of the aggregation were supposed to be here by now (see the method to obtain the waiting_time below) */
	if(req->already_waited)
		return 0;
	
	/*first we need to fing the predicted_aggregation_start.*/
	if((req->reqnb == 1) && /*it is not an aggregated request*/ 
           (req->mirror))  /*but we have a predicted aggregation to it*/
		predicted_head = req->mirror->predicted_aggregation_start;
	else /*it is an aggregated request*/
	{
		agios_list_for_each_entry(tmp, &req->reqs_list, aggregation_element) /*go through all subreqs*/
		{
			if((tmp->mirror) && (tmp->mirror->predicted_aggregation_start) && (tmp->mirror->predicted_aggregation_start != predicted_head)) //we have a predicted aggregation to this request, and it is not the one we've already found 
			{
				if((!predicted_head) || ((predicted_head) && (predicted_head->predicted_aggregation_size < tmp->mirror->predicted_aggregation_start->predicted_aggregation_size))) /*this new aggregation is larger*/
					predicted_head = tmp->mirror->predicted_aggregation_start;
			}
		}
	}
	if(!predicted_head)
		return 0;
	
	/*did we predict a better aggregation than what was done?*/
	if(predicted_head->predicted_aggregation_size > req->reqnb)
	{
		/*how much time has passed since the requests from this aggregation started arriving?*/
		max = get_nanoelapsed_llu(req->jiffies_64);

		/*how much time will pass while all the requests from the predicted aggregation arrive?*/
		pmax = predicted_head->last_agg_time - predicted_head->first_agg_time;

		/*so how much should we wait?*/
		if(pmax > max)
		{
			waiting_time = pmax - max;	
			if(waiting_time > WAIT_PREDICTED_BETTER)
			{
#ifdef AGIOS_DEBUG
				agios_print("was going to wait for %llu, but changed it to %d\n", waiting_time, WAIT_PREDICTED_BETTER);
#endif
				waiting_time = WAIT_PREDICTED_BETTER;
			}
		}
		else /*it does not make sense, all the requests should be here by now!*/
			waiting_time = WAIT_PREDICTED_BETTER;  /*TODO or maybe change it to a smaller waiting_time since we should not even be waiting*/
		
	}
	if(waiting_time > 0)
		req->already_waited=1; //so we won't make the same decision again
	return waiting_time;
}
/*use the predicted access pattern to make a decision about the scheduling algorithm to use.*/
int predict_select_best_algorithm(void)
{
	if(predicted_ap_operation == -1) /*we should not choose if we weren't able to detect the access pattern*/
		return -1;
		
	return get_algorithm_from_string(scheduling_algorithm_selection_tree(predicted_ap_operation, predicted_ap_fileno, get_access_ratio(predicted_ap_server_reqsize, predicted_ap_operation), predicted_ap_spatiality, predicted_ap_reqsize));
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//FUNCTIONS RELATED TO ACCESS PATTERN DETECTION
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/*upon adding a request, considers its offset difference from the previous request to the same queue*/
void update_average_distance(struct related_list_t *related_list, long long offset, long len)
{
	long int this_distance;
	
	if(related_list->lastfinaloff > 0)
	{
		if(related_list->avg_distance == -1)
			related_list->avg_distance = 0;
		this_distance = offset - related_list->lastfinaloff;
		if(this_distance < 0)
			this_distance *= -1;
		this_distance = this_distance/len;
		related_list->avg_distance += this_distance;
		related_list->avg_distance_count++;
	}
	related_list->lastfinaloff = offset+len;
}
/*calculates the alpha factor, that represents the workload's provided ability to overlap waiting times with processing of other requests. More detail in the ICPADS 2013 paper or in my thesis*/
void calculate_prediction_alpha(unsigned long long int time_spent_waiting, unsigned long long int waiting_time_overlapped, short int first_prediction)
{

	if(first_prediction)
	{
		/*we don't have any measures of waiting times overlapping because the scheduler just started. we will have to estimate it*/
		struct agios_list_head *req_l, *req_l_next;
		struct request_t *req, *req_next;
		unsigned long long int time_between;
		unsigned long long int A, B;

		A = B = 0;
		req_l = predict_timeline.next;
		while((req_l) && (req_l != &predict_timeline))
		{
			req = agios_list_entry(req_l, struct request_t, timeline);
			time_between = 0;

			req_l_next = req_l->next;
			while((req_l_next) && (req_l_next != &predict_timeline))
			{
				req_next = agios_list_entry(req_l_next, struct request_t, timeline);
				if(CHECK_AGGREGATE(req, req_next))
				{
					/*we found the next contiguous request, the search is over for this one*/
					A += AVERAGE_WAITING_TIME;
					if(time_between > 0)
					{
						if(time_between >= AVERAGE_WAITING_TIME)
						{
							/*full overlapping*/
							B+= AVERAGE_WAITING_TIME; 	
						}
						else
						{
							/*partial overlapping*/
							B+= time_between;
						}
					}
					break;
				}
				else
				{
					/*still between contiguous requests*/
					time_between += get_access_time(req_next->io_data.len, req_next->type);
				}
				req_l_next = req_l_next->next;
			} 
			req_l = req_l->next;
		}
		if(A == 0)
			prediction_alpha = 0.0;
		else
			prediction_alpha = ((long double) B)/((long double) A);
		
	}
	else
	{
		/*we can use the real measurements from the execution of the application until now*/
		if(time_spent_waiting == 0)
			prediction_alpha = 0.0;
		else
			prediction_alpha = ((long double) waiting_time_overlapped) / ((long double) time_spent_waiting);
	}
	debug("prediction_alpha is now %Le", prediction_alpha);
}

/*see if req can be added to the aggregation that starts at agg_head
 * start offset of req MUST be larger or equal than the start offset of the aggregation. 
 * since the lists are in offset order and we look from the first, this should not be a problem. */
/* for more detail about this algorithm, please see our paper from ICPADS 2013 (AGIOS: Application-guided I/O Scheduling for Parallel File Systems)*/
int aggregation_is_possible(long int current_size, struct request_t *agg_head, struct request_t *req)
{
	unsigned long long int delta;

	/*see if they are contiguous*/
	if(agg_head->io_data.offset + current_size < req->io_data.offset)
	{
		return 0; /*they are not*/
	}
	else
	{
		/*find out how much time we have to wait between them*/
		if(req->jiffies_64 >= agg_head->last_agg_time)
			delta = req->jiffies_64 - agg_head->last_agg_time;
		else
			delta = agg_head->last_agg_time - req->jiffies_64;
		
		
		if( ( get_access_time(current_size,req->type) + get_access_time(req->io_data.len, req->type) ) > 
                    ( get_access_time(current_size + req->io_data.len, req->type) + (delta * (1.0 - prediction_alpha)) ) )
		{
			/*the time to process the requests separately is larger than the time to process the two requests together plus the waiting time between them. Therefore, we should wait to process them together!*/
			return 1;
		}
		else
		{
			/*they are contiguous, but the time between can be large enough to make the aggregation impossible*/
			return 0;
		}
	}
}
/*go through all requests from a queue trying to build aggregations between them*/
void predict_aggregations_onlist(struct related_list_t *predicted_l, short int cleanup)
{
	struct request_t *req, *req_next;
	long int current_size=0;
	struct agios_list_head *req_l, *req_l_next;

	if(cleanup)
	{
		agios_list_for_each_entry(req, &predicted_l->list, related)
		{
			req->predicted_aggregation_size = 1;
			req->predicted_aggregation_start = NULL;
			req->already_waited=0;  
		}
	}

	req_l = predicted_l->list.next;
	while((req_l) && (req_l != &(predicted_l->list)))
	{
		req = agios_list_entry(req_l, struct request_t, related);		

		if(req->predicted_aggregation_start == NULL)
		{
			/*so this is the start of a new aggregation*/
			current_size = req->io_data.len;
			req->predicted_aggregation_size=1;
			req->predicted_aggregation_start = req;
			req->first_agg_time = req->jiffies_64;
			req->last_agg_time = req->jiffies_64;

			req_l_next = req_l->next;
			while(req_l_next && (req_l_next != &(predicted_l->list)))
			{
				req_next = agios_list_entry(req_l_next, struct request_t, related);
				if(req->io_data.offset + current_size < req_next->io_data.offset)
				{
					/*if they are not contiguous, then none of the next requests on the lists will be contiguous to this one since they are ordered by offset*/
					break;
				}
				if((req_next->predicted_aggregation_start == NULL) && aggregation_is_possible(current_size, req, req_next))
				{
					/*aggregate them!*/
					req->predicted_aggregation_size++;
					req_next->predicted_aggregation_start = req;
					req->globalinfo->stats.aggs_no++;
					if(req_next->jiffies_64 < req->first_agg_time)
						req->first_agg_time = req_next->jiffies_64;
					if(req_next->jiffies_64 > req->last_agg_time)
						req->last_agg_time = req_next->jiffies_64;
					/*update current_size*/
					current_size = current_size + req_next->io_data.len - (req->io_data.offset + current_size - req_next->io_data.offset);
				}
			
				req_l_next = req_l_next->next;
			} 
			req->globalinfo->stats.sum_of_agg_reqs+= req->predicted_aggregation_size;
			if(req->predicted_aggregation_size > req->globalinfo->stats.biggest)
				req->globalinfo->stats.biggest = req->predicted_aggregation_size;

		}
		req_l = req_l->next;
	}
}

/*call the predict_aggregations_onlist function to all the lists of predicted requests*/
void predict_aggregations(short int cleanup)
{
	int i;
	struct agios_list_head *hash_list;
	struct request_file_t *req_file;
	
	for(i=0; i< hashtable_get_size(); i++)
	{
		hash_list = hashtable_lock(i);

		if(!agios_list_empty(hash_list))
		{
			/*go through all the files of this hash value*/
			agios_list_for_each_entry(req_file, hash_list, hashlist)
			{
				if(!agios_list_empty(&(req_file->predicted_reads.list)))	
				{
					/*the predicted reads list*/
					predict_aggregations_onlist(&req_file->predicted_reads, cleanup);
				}
				if(!agios_list_empty(&(req_file->predicted_writes.list)))
				{
					/*the predicted writes list*/
					predict_aggregations_onlist(&req_file->predicted_writes, cleanup);
				}
				if((config_get_trace_predict()) && (!(agios_list_empty(&(req_file->predicted_writes.list)) && agios_list_empty(&(req_file->predicted_reads.list)))))
					agios_trace_print_predicted_aggregations(req_file);
			}
		}

		hashtable_unlock(i);
	}
}

/*calculate the "average stripe access time difference"
 *for that we go through all requests inside a queue considering one stripe at a time. 
 *Between the requests from this stripe, we calculate the difference between the maximum and the minimum arrival times. 
 *Then we take the average between all stripes' differences
 */
void calculate_average_stripe_access_time_difference(struct related_list_t *related_list) 
{
	unsigned long long int arrival_times_differences_sum, max_arrival_time, min_arrival_time;
	int current_stripe, current_count, stripes_count;
	struct request_t *req;
	
	if(!agios_list_empty(&related_list->list))
	{
		current_stripe = 0;
		current_count = 0;
		min_arrival_time = ~0;
		max_arrival_time = 0;
		arrival_times_differences_sum=0;
		stripes_count=0;
		agios_list_for_each_entry(req, &related_list->list, related)
		{
			if((req->io_data.offset / config_get_stripesize()) != current_stripe)
			{
				//since requests are ordered by offset, we know we already went through all requests from the current stripe
				current_stripe = (req->io_data.offset / config_get_stripesize());
				if(current_count > 1)
				{
					arrival_times_differences_sum = max_arrival_time - min_arrival_time; //get the difference from the last stripe if we had at least two requests in it
					stripes_count++;
				}
				current_count = 0;
				min_arrival_time = ~0;
				max_arrival_time = 0;
			}
			/*update min and max*/
			if(req->jiffies_64 > max_arrival_time)
				max_arrival_time = req->jiffies_64;
			if(req->jiffies_64 < min_arrival_time)
				min_arrival_time = req->jiffies_64;
			current_count++;
		} 
		if(current_count > 1) //we've finished going through all requests while treating the last stripe
		{
			arrival_times_differences_sum = max_arrival_time - min_arrival_time;
			stripes_count++;
		}
		/*now we have the metric*/
		if(stripes_count > 0)
			related_list->avg_stripe_difference =  (arrival_times_differences_sum/stripes_count)/1000000; //ms		
		else
			related_list->avg_stripe_difference=-1;
	}
}

inline short int get_index_max(int *count)
{
	if(count[0] >= count[1])
		return 0;
	else
		return 1;
}
inline void print_access_pattern(char * operation)
{
	agios_just_print("Detected from the majority an access pattern of %s, to %ld files, server_reqsize is %lld, spatiality is ", operation, predicted_ap_fileno, predicted_ap_server_reqsize);
	if(predicted_ap_spatiality ==AP_CONTIG)
		agios_just_print("CONTIGUOUS");
	else
		agios_just_print("NON-CONTIGUOUS");
	agios_just_print(", applications requests are ");
	if(predicted_ap_reqsize == AP_SMALL)
		agios_just_print("SMALL");
	else
		agios_just_print("LARGE");
	agios_just_print(".\n");
}
/*1. updates the "average distance between consecutive requests" metric to all files with predicted requests. 
 *The avg_distance variable received the sum of the differences between every pair of consecutive requests while reading them, so we have to divide it by avg_distance_count to get the average */
/*2. calculates the "average stripe arrival time difference" metric to all files with predicted requests. */
/*3. uses 1, 2, and a decision tree to detect access pattern's spatiality and application request size aspects*/
//TODO if we read new traces during execution, their average will dominate the first one because we are calculating average in parts. Is this what we want?
void update_traced_access_pattern()
{
	int i;
	struct agios_list_head *hash_list;
	struct request_file_t *req_file;
	long int reads_count=0;
	long int writes_count=0;
	int read_spatiality_count[2] = {0,0};
	int write_spatiality_count[2] = {0,0};
	int read_reqsize_count[2] = {0,0};
	int write_reqsize_count[2] = {0,0};
	long long read_server_reqsize = 0;
	long long write_server_reqsize = 0;

	
	
	PRINT_FUNCTION_NAME;
	
	for(i=0; i< hashtable_get_size(); i++)
	{
		hash_list = hashtable_lock(i);

		if(!agios_list_empty(hash_list))
		{
			/*go through all the files of this hash value*/
			agios_list_for_each_entry(req_file, hash_list, hashlist)
			{
				if((!agios_list_empty(&req_file->predicted_reads.list)) || (req_file->predicted_reads.avg_distance > -1))
				{
					if(req_file->predicted_reads.avg_distance_count > 1)
					{
						req_file->predicted_reads.avg_distance= ((long double) req_file->predicted_reads.avg_distance)/req_file->predicted_reads.avg_distance_count;
						req_file->predicted_reads.avg_distance_count=1;
					}

					calculate_average_stripe_access_time_difference(&req_file->predicted_reads);

					if((req_file->predicted_reads.avg_stripe_difference >= 0) && (req_file->predicted_reads.avg_distance >= 0))
					{
						reads_count++;
						read_server_reqsize += req_file->predicted_reads.stats.total_req_size / req_file->predicted_reads.proceedreq_nb;

						access_pattern_detection_tree(RT_READ, req_file->predicted_reads.avg_distance, req_file->predicted_reads.avg_stripe_difference, &req_file->predicted_reads.spatiality, &req_file->predicted_reads.app_request_size);
						read_spatiality_count[req_file->predicted_reads.spatiality]++;
						read_reqsize_count[req_file->predicted_reads.app_request_size]++;
					}
				}
				if((!agios_list_empty(&req_file->predicted_writes.list)) || (req_file->predicted_writes.avg_distance > -1))
				{
					if(req_file->predicted_writes.avg_distance_count > 1)
					{
						req_file->predicted_writes.avg_distance= req_file->predicted_writes.avg_distance/req_file->predicted_writes.avg_distance_count;
						req_file->predicted_writes.avg_distance_count=1;
					}

					calculate_average_stripe_access_time_difference(&req_file->predicted_writes);	
			
					if((req_file->predicted_writes.avg_stripe_difference >= 0) && (req_file->predicted_writes.avg_distance >= 0))
					{
						writes_count++;
						write_server_reqsize += req_file->predicted_writes.stats.total_req_size / req_file->predicted_writes.proceedreq_nb;

						access_pattern_detection_tree(RT_WRITE, req_file->predicted_writes.avg_distance, req_file->predicted_writes.avg_stripe_difference, &req_file->predicted_writes.spatiality, &req_file->predicted_writes.app_request_size);
						write_spatiality_count[req_file->predicted_writes.spatiality]++;
						write_reqsize_count[req_file->predicted_writes.app_request_size]++;
					}
				}
	
			}
		}

		hashtable_unlock(i);
	}
	/*take the access pattern from the majority*/
	if(reads_count > writes_count)
	{
		predicted_ap_operation = RT_READ;
		predicted_ap_fileno = reads_count;
		predicted_ap_spatiality = get_index_max(read_spatiality_count);
		predicted_ap_reqsize = get_index_max(read_reqsize_count);
		predicted_ap_server_reqsize = round(((float) read_server_reqsize) / reads_count);
		print_access_pattern("READS");
		
	}
	else if (writes_count > 0)
	{
		predicted_ap_operation = RT_WRITE;
		predicted_ap_fileno = writes_count;
		predicted_ap_spatiality = get_index_max(write_spatiality_count);
		predicted_ap_reqsize = get_index_max(write_reqsize_count);
		predicted_ap_server_reqsize = round(((float) write_server_reqsize)/writes_count);
		print_access_pattern("WRITES");
	}
}
/*when combining multiple versions of a predicted requests (when it appears in multiple trace files), jiffies_64 receives the sum of all versions' arrival times, and reqnb counts them. So after reading trace files we need to call this function to make the division so jiffies_64 can be used as this request's predicted arrival time. We do it like this because partial averages would make the last considered occurrences have more weight than the first ones.*/
void update_requests_arrival_times()
{
	int i;
	struct agios_list_head *hash_list;
	struct request_file_t *req_file;
	struct request_t *req;
	
	
	PRINT_FUNCTION_NAME;
	
	for(i=0; i< hashtable_get_size(); i++)
	{
		hash_list = hashtable_lock(i);

		if(!agios_list_empty(hash_list))
		{
			/*go through all the files of this hash value*/
			agios_list_for_each_entry(req_file, hash_list, hashlist)
			{
				if(!agios_list_empty(&req_file->predicted_reads.list))
				{
					agios_list_for_each_entry(req, &req_file->predicted_reads.list, related)
					{
						if(req->reqnb > 1)
						{
							req->jiffies_64 = req->jiffies_64 /req->reqnb;
							req->reqnb = 1;
						}
					}
				}
				if(!agios_list_empty(&req_file->predicted_writes.list))
				{
					agios_list_for_each_entry(req, &req_file->predicted_writes.list, related)
					{
						if(req->reqnb > 1)
						{
							req->jiffies_64 = req->jiffies_64 /req->reqnb;
							req->reqnb = 1;
						}
					}
				}
				req_file->first_request_predicted_time = -1;
			}
		}
		hashtable_unlock(i);
	}

				

}
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//FUNCTIONS RELATED TO READING TRACE FILES
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void reset_files_first_arrival_time()
{
	int i;
	struct agios_list_head *hash_list;
	struct request_file_t *req_file;
	
	PRINT_FUNCTION_NAME;
	
	for(i=0; i< hashtable_get_size(); i++)
	{
		hash_list = hashtable_lock(i);

		if(!agios_list_empty(hash_list))
		{
			/*go through all the files of this hash value*/
			agios_list_for_each_entry(req_file, hash_list, hashlist)
			{
				req_file->first_request_predicted_time = -1;
				req_file->predicted_reads.lastfinaloff=0;
				req_file->predicted_writes.lastfinaloff=0;
			}
		}
		hashtable_unlock(i);
	}
}
/* read tracefiles, put all requests into the data structures (hashtable and timeline), and calculates metrics
 * last = the number of files from which we already got predictions (in the first execution it must 
 * be 0, and in the next ones it will be the tracefile_counter)
 * files_nb = the number of existing tracefiles*/
void read_predictions_from_traces(int last, int files_nb)
{
	FILE *input_file;

	unsigned long long int timestamp;
	char filename[300];
	char operation[300];
	int type;
	long long offset;
	long len;
	int ret=5;
	int i;

	if(files_nb <= last)
		return;

	for(i=last+1; i<= files_nb; i++)
	{
		snprintf(filename, 300*sizeof(char), "%s.%d.%s", config_get_trace_file_name(1), i, config_get_trace_file_name(2));

		input_file = fopen(filename, "r");

		if(!input_file)
			continue;

		debug("reading from tracefile %s\n", filename);
		/*read predictions from the file*/
		while((ret = fscanf(input_file, "%llu\t%s\t%s\t%lld\t%ld\n", &timestamp, filename, operation, &offset, &len)) == 5) //it will not work if the trace was generated with the FULL_TRACE option. That is for debug only
		{
			if(strcmp(operation, "R") == 0)
				type = RT_READ;
			else
				type = RT_WRITE;
			add_prediction(filename, type, offset, len, timestamp);
		}
		if(ret > 0)
			agios_print("Error while reading the trace file %s.%d.%s. Stopping for this file.\n", config_get_trace_file_name(1), i, config_get_trace_file_name(2));
	
		/*we have to reset the first arrival times for the files because the next trace can have accesses to the same file, and it would be harder to identify duplicate predictions*/
		reset_files_first_arrival_time();
			
		fclose(input_file);
	}	
	/*we have to take the average between repeated predictions' arrival times*/
	update_requests_arrival_times();
		
}
/* start = the number of tracefiles we already know exist*/
int how_many_tracefiles_there_are(int start)
{
	FILE *input_file=NULL;
	char filename[300];
	int ret=start+1;

	do{
		snprintf(filename, 300*sizeof(char),  "%s.%d.%s", config_get_trace_file_name(1), ret, config_get_trace_file_name(2));
		input_file = fopen(filename, "r");
		if(input_file)
		{
			debug("%s is a valid trace file\n", filename);
			fclose(input_file);
			ret++;
		}
		else
		{
			debug("%s is NOT a valid trace file \n", filename);
		}
	} while(input_file);

	return ret-1;
}
//return the number of files for which we obtained information from simplified traces
int read_predictions_from_simple_traces(void)
{
	FILE *input_file=NULL;
	char filename[300];
	int ret;
	char file_id[300];
	struct agios_list_head *hash_list;
	unsigned long hash_val;
	struct request_file_t *req_file;
	int count =0;
	

	PRINT_FUNCTION_NAME;
	if((config_get_simple_trace_file_prefix() == NULL) || (config_get_trace_file_name(2) == NULL))
		return 0;
	do{
		simple_tracefile_counter++;
		snprintf(filename, 300*sizeof(char), "%s.%d.%s", config_get_simple_trace_file_prefix(), simple_tracefile_counter, config_get_trace_file_name(2));
		input_file = fopen(filename, "r");
		if(input_file)
		{
			debug("%s is a valid trace file ", filename);
			ret = fscanf(input_file, "%s\nREAD\t", file_id);
			if(ret != 1)
			{
				agios_print("could not read from simplified trace file %s\n", filename);
				fclose(input_file);
				continue;
			}
			debug("for the file %s\n", file_id);

			hash_val = AGIOS_HASH_STR(file_id) % AGIOS_HASH_ENTRIES;
			hash_list = hashtable_lock(hash_val);
			req_file = find_req_file(hash_list, file_id, RS_NONE);

			ret = fscanf(input_file, "%Le\t%Le\t%lld\nWRITE\t%Le\t%Le\t%lld\n", &req_file->predicted_reads.avg_distance, &req_file->predicted_reads.avg_stripe_difference, &req_file->predicted_reads.stats.total_req_size, &req_file->predicted_writes.avg_distance, &req_file->predicted_writes.avg_stripe_difference, &req_file->predicted_writes.stats.total_req_size);
			if(ret != 6)
			{
				agios_print("could not read from simplified trace file %s\n", filename);
				fclose(input_file);
				hashtable_unlock(hash_val);
				continue;
			}
			debug("READ\t%Le\t%Le\t%lld\tWRITE\t%Le\t%Le\t%lld\n", req_file->predicted_reads.avg_distance, req_file->predicted_reads.avg_stripe_difference, req_file->predicted_reads.stats.total_req_size, req_file->predicted_writes.avg_distance, req_file->predicted_writes.avg_stripe_difference, req_file->predicted_writes.stats.total_req_size);
			req_file->wrote_simplified_trace=1; //so we won't write it to another file
			req_file->predicted_reads.proceedreq_nb = req_file->predicted_writes.proceedreq_nb = 1;
			count++;

			hashtable_unlock(hash_val);
			fclose(input_file);
		}
		else
		{
			debug("%s is NOT a valid trace file \n", filename);
		}
	} while(input_file);
	return count;

}
void write_simplified_trace_files(void)
{
	int i;
	struct agios_list_head *hash_list;
	struct request_file_t *req_file;
	FILE *output_file=NULL;
	char filename[300];

	for(i=0; i< hashtable_get_size(); i++)
	{
		hash_list = hashtable_lock(i);

		if(!agios_list_empty(hash_list))
		{
			/*go through all the files of this hash value*/
			agios_list_for_each_entry(req_file, hash_list, hashlist)
			{
				if(!req_file->wrote_simplified_trace)
				{
					if((!agios_list_empty(&(req_file->predicted_reads.list))) || (!agios_list_empty(&(req_file->predicted_writes.list)))) //if we have any predicted requests. we are not generating simplified trace files from information we got from simplified trace files...
					{
						snprintf(filename, 300*sizeof(char), "%s.%d.%s", config_get_simple_trace_file_prefix(), simple_tracefile_counter, config_get_trace_file_name(2));
						output_file = fopen(filename, "w");
						if(!output_file)
						{
							agios_print("Could not generate simplified trace file %s\n", filename);
							perror("fopen");
							continue;
						}
						simple_tracefile_counter++;
						req_file->wrote_simplified_trace=1; //so we won't write it again
						fprintf(output_file, "%s\n", req_file->file_id);
						if(agios_list_empty(&(req_file->predicted_reads.list)))
							fprintf(output_file, "READ\t-1\t-1\t0\n");
						else
							fprintf(output_file, "READ\t%Le\t%Le\t%lld\n", req_file->predicted_reads.avg_distance, req_file->predicted_reads.avg_stripe_difference, req_file->predicted_reads.stats.total_req_size / req_file->predicted_reads.proceedreq_nb);
						if(agios_list_empty(&(req_file->predicted_writes.list)))
							fprintf(output_file, "WRITE\t-1\t-1\t0\n");
						else
							fprintf(output_file, "WRITE\t%Le\t%Le\t%lld\n", req_file->predicted_writes.avg_distance, req_file->predicted_writes.avg_stripe_difference, req_file->predicted_writes.stats.total_req_size / req_file->predicted_writes.proceedreq_nb);
						fclose(output_file);
					}
				}
			}
		}

		hashtable_unlock(i);
	}
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//PREDICTION THREAD'S MAIN FUNCTIONS
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void *prediction_thr(void *arg)
{
	int new_tracefile_counter;
	struct timespec predict_init_start;

	pthread_mutex_lock(&prediction_thr_refresh_mutex);	
	agios_gettime(&predict_init_start);
	
	if(tracefile_counter == -1)
	{
		/*we still do not know how many trace files there are*/
		tracefile_counter = how_many_tracefiles_there_are(0);
	}

	init_agios_list_head(&predict_timeline); //we used to have both a timeline and a hashtable for the schedulers, but it led to race conditions, since you could have the lock to a line of the hashtable and with it change the timeline, where there are requests from other lines. I'm still not 100% positive it will not be a problem for the prediction module... it's probably ok, because we'll never erase predictions (unlike requests which are processed and then go away)

	if((tracefile_counter > 0) && (config_get_predict_read_traces()))
	{
		read_predictions_from_traces(0,tracefile_counter);
		read_predictions_from_simple_traces();
		update_traced_access_pattern();
		if(config_get_predict_request_aggregation())
		{
			calculate_prediction_alpha(0, 0, 1); /*necessary to the prediction of aggregations*/
			predict_aggregations(0);  /*go through all the predictions to evaluate possible aggregations*/
		}
		if(config_get_write_simplified_traces())
			write_simplified_trace_files();
	}
	else
	{
		if(read_predictions_from_simple_traces() > 0)
			update_traced_access_pattern();
	}
	predict_init_time = get_nanoelapsed(predict_init_start);
	debug("finished reading from %d traces in %llu ns\n", tracefile_counter, predict_init_time);
	signal_predict_init();
	pthread_mutex_unlock(&prediction_thr_refresh_mutex);	

	/*wait for refresh requests*/
	do {
		pthread_mutex_lock(&prediction_thr_refresh_mutex);	
		pthread_cond_wait(&prediction_thr_refresh_cond, &prediction_thr_refresh_mutex);
		pthread_mutex_unlock(&prediction_thr_refresh_mutex);

		if(!prediction_thr_should_stop())
		{
			pthread_mutex_lock(&prediction_thr_refresh_mutex);	
			if(new_trace_file)
			{
				/*read new predictions before recalculating everything*/

				new_trace_file = 0;
				new_tracefile_counter = how_many_tracefiles_there_are(tracefile_counter);
				new_tracefile_counter--; //we take one out, because it's the one the Trace Module is using right now.
				
				if(config_get_predict_read_traces())
				{
					read_predictions_from_traces(tracefile_counter, new_tracefile_counter);
					update_traced_access_pattern();
					if(config_get_write_simplified_traces())
						write_simplified_trace_files();
				}
				tracefile_counter = new_tracefile_counter;
			}
			if(config_get_predict_request_aggregation())
			{
				/*re-calculate alpha and re-do all agregations predictions*/
				calculate_prediction_alpha(get_time_spent_waiting(), get_waiting_time_overlapped(), 0);
				predict_aggregations(1);
			}
			pthread_mutex_unlock(&prediction_thr_refresh_mutex);
		}
	} while(!prediction_thr_should_stop());

	return 0;
} 

int prediction_module_init(int file_counter)
{
	int ret;

	tracefile_counter = file_counter;
	current_predicted_reqfilenb=0;
	ret = agios_start_thread(prediction_thread, prediction_thr, "agios_prediction_thread", NULL);
	if(ret != 0)
		agios_print("Unable to start a thread for the prediction module of agios!\n");
	return ret;
}
#endif
