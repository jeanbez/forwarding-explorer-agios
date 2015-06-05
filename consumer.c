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

void process_requests(struct request_t *head_req, struct client *clnt, int hash)
{
	struct request_t *req;
	struct request_file_t *req_file = head_req->globalinfo->req_file;
	
	PRINT_FUNCTION_NAME;
	if(!head_req)
		return; /*whaaaat?*/

	if(config_get_trace())
		agios_trace_process_requests(head_req);
	debug("returning %u requests to the file system!", head_req->reqnb);
	

	if((clnt->process_requests) && (head_req->reqnb > 1))
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
			debug("request [%d] - size %ld - going back to the file system", req->data, req->io_data.len);
			reqs[reqs_index]=req->data;
			reqs_index++;
		}
		head_req->globalinfo->current_size -= head_req->io_data.len;
		clnt->process_requests(reqs, head_req->reqnb);
		dec_many_current_reqnb(hash, head_req->reqnb);
		free(reqs);
	}
	else{
		if(head_req->reqnb == 1)
		{
			VERIFY_REQUEST(head_req);
			debug("request [%d]  - size %ld - going back to the file system", head_req->data, head_req->io_data.len);
			head_req->globalinfo->current_size -= head_req->io_data.len; 
		
			clnt->process_request(head_req->data);
			dec_current_reqnb(hash);
		}
		else
		{
			agios_list_for_each_entry(req, &head_req->reqs_list, aggregation_element)
			{	
				VERIFY_REQUEST(req);
				debug("request [%d] - size %ld - going back to the file system", req->data, req->io_data.len);
				req->globalinfo->current_size -= req->io_data.len; 
				clnt->process_request(req->data);
			}
			dec_many_current_reqnb(hash, head_req->reqnb);
		}
	}
	if((req_file->related_reads.current_size == 0) && (req_file->related_writes.current_size == 0)) //this file just ran out of reqs
		dec_current_reqfilenb();

	if(processed_period > 0)
	{
		processed_reqnb += head_req->reqnb;
		if(processed_reqnb > processed_period)
		{
			processed_reqnb = 0;
			refresh_predictions(); //it's time to recalculate the alpha factor and review all predicted requests aggregation
		}
	}

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


