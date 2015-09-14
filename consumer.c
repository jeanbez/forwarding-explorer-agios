/* File:	consumer.c
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
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

#ifndef AGIOS_KERNEL_MODULE
#include <pthread.h>
#else
#include <linux/kthread.h>
#endif


//so we can let the AGIOS thread know we have new requests (so it can schedule them)
#ifdef AGIOS_KERNEL_MODULE
static struct completion request_added;
#else
static pthread_cond_t request_added_cond;
static pthread_mutex_t request_added_mutex;
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
static struct completion exited;
#endif

void consumer_init()
{
#ifdef AGIOS_KERNEL_MODULE
	init_completion(&exited);
	init_completion(&request_added);
#else
	agios_cond_init(&request_added_cond);
	agios_mutex_init(&request_added_mutex);
#endif
	
}


static int processed_reqnb=0;
static int processed_period = -1;

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

inline void update_filenb_counter(int hash, struct request_file_t *req_file)
{
	//if this was the last request for this file, we remove it from the counter that keeps track of the number of files being accessed right now. If requests are being held at the hashtable, we check that through the related lists. If requests are being held at the timeline, we use the counter
	if((hash >= 0) && (req_file->related_reads.current_size == 0) && (req_file->related_writes.current_size == 0))
		dec_current_reqfilenb();
	else if((hash == -1) && (req_file->timeline_reqnb == 0))
		dec_current_reqfilenb();
}

void process_requests(struct request_t *head_req, struct client *clnt, int hash)
{
	struct request_t *req;
	struct request_file_t *req_file = head_req->globalinfo->req_file;
	
	PRINT_FUNCTION_NAME;
	if(!head_req)
		return; /*whaaaat?*/

	if(config_get_trace())
		agios_trace_process_requests(head_req);

	if((clnt->process_requests) && (head_req->reqnb > 1)) //if the user has a function capable of processing a list of requests at once and this is an aggregated request
	{
#ifdef ORANGEFS_AGIOS
		int64_t *reqs = (int64_t *)malloc(sizeof(int64_t)*(head_req->reqnb+1));
#else
		int *reqs = (int *)malloc(sizeof(int)*(head_req->reqnb+1));
#endif
		int reqs_index=0;
		agios_list_for_each_entry(req, &head_req->reqs_list, aggregation_element)
		{
			VERIFY_REQUEST(req);
			debug("request [%d] - size %ld, offset %lld, file %s - going back to the file system", req->data, req->io_data.len, req->io_data.offset, req->file_id);
			reqs[reqs_index]=req->data;
			reqs_index++;
			req->globalinfo->current_size -= req->io_data.len; //when we aggregate overlapping requests, we don't adjust the related list current_size, since it is simply the sum of all requests sizes. For this reason, we have to subtract all requests from it individually when processing a virtual request.
		}
		//we have to update the counters before releasing the lock to process requests, otherwise the lock may be obtained by another thread to add a new request and end up in weird behaviors.
		head_req->globalinfo->req_file->timeline_reqnb-=head_req->reqnb;
		update_filenb_counter(hash, req_file);
		dec_many_current_reqnb(hash, head_req->reqnb);
		unlock_structure_mutex(hash);
		clnt->process_requests(reqs, head_req->reqnb);
		lock_structure_mutex(hash);
		free(reqs);
	}
	else{
		if(head_req->reqnb == 1)
		{
			VERIFY_REQUEST(head_req);
			debug("request [%d]  - size %ld, offset %lld, file %s - going back to the file system", head_req->data, head_req->io_data.len, head_req->io_data.offset, head_req->file_id);
			head_req->globalinfo->current_size -= head_req->io_data.len; 
			head_req->globalinfo->req_file->timeline_reqnb--;
			update_filenb_counter(hash, req_file);
			dec_current_reqnb(hash);
			unlock_structure_mutex(hash); //holding this lock while waiting for requests processing can cause a deadlock if the user has a mutex to avoid adding and consuming requests at the same time (it will be stuck adding a request while we are stuck waiting for a request to be processed)
			clnt->process_request(head_req->data);
			lock_structure_mutex(hash);
		}
		else
		{
			agios_list_for_each_entry(req, &head_req->reqs_list, aggregation_element)
			{	
				VERIFY_REQUEST(req);
				debug("request [%d] - size %ld, offset %lld, file %s - going back to the file system", req->data, req->io_data.len, req->io_data.offset, req->file_id);
				req->globalinfo->current_size -= req->io_data.len; 
				req->globalinfo->req_file->timeline_reqnb--;
				update_filenb_counter(hash, req_file);
				dec_current_reqnb(hash);
				unlock_structure_mutex(hash); 
				clnt->process_request(req->data);
				lock_structure_mutex(hash); 
			}
		}
	}

	if(processed_period > 0)
	{
		processed_reqnb += head_req->reqnb;
		if(processed_reqnb > processed_period)
		{
			processed_reqnb = 0;
			refresh_predictions(); //it's time to recalculate the alpha factor and review all predicted requests aggregatio
		}
	}
	
	if(hash >= 0)
		debug("current status. hashtable[%d] has %d requests, there are %d requests in the scheduler to %d files.", hash, get_hashlist_reqcounter(hash), get_current_reqnb(), get_current_reqfilenb());
	PRINT_FUNCTION_EXIT;
}

#ifdef AGIOS_KERNEL_MODULE
int agios_thread(void *arg)
#else
void * agios_thread(void *arg)
#endif
{
	struct consumer_t *consumer = arg;
	short int stop=0;

#ifndef AGIOS_KERNEL_MODULE
	struct timespec timeout_tspec;

#endif
	processed_period = config_get_prediction_recalculate_alpha_period();


	do {
		if(get_current_reqnb() == 0)
		{
			/* there are no requests to be processed, so we just have to wait until a new request arrives. 
			 * We use a timeout to avoid a situation where we missed the signal and will sleep forever, and
                         * also because we have to check once in a while to see if we should end the execution.
			 */
#ifdef AGIOS_KERNEL_MODULE
			wait_for_completion_timeout(&consumer->request_added, WAIT_SHIFT_CONST);
#else
			/*fill the timeout structure*/
			timeout_tspec.tv_sec =  WAIT_SHIFT_CONST / 1000000000L;
			timeout_tspec.tv_nsec = WAIT_SHIFT_CONST % 1000000000L;
			
 			pthread_mutex_lock(&consumer->request_added_mutex);
			pthread_cond_timedwait(&consumer->request_added_cond, &consumer->request_added_mutex, &timeout_tspec);
			pthread_mutex_unlock(&consumer->request_added_mutex);
#endif
		}
	
		if(!(stop = agios_thread_should_stop())) {
			if(get_current_reqnb() > 0)
			{
				consumer->io_scheduler->schedule(consumer->client);
			}
		}
        } while (!stop);

	consumer->task = 0;
#ifdef AGIOS_KERNEL_MODULE
	complete(&consumer->exited);
#endif
	return 0;
}


