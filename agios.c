/* File:	agios.c
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the interface to its users
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
#include "consumer.h"
#include "predict.h"
#include "trace.h"
#include "common_functions.h"
#include "proc.h"
#include "agios_config.h"
#include "estimate_access_times.h"

#ifdef AGIOS_KERNEL_MODULE
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#endif

#ifdef AGIOS_KERNEL_MODULE
static struct task_struct *agios_pthread;
static short int agios_in_use=0;
#else
static pthread_t agios_pthread;
extern int agios_thread_stop;
extern pthread_mutex_t agios_thread_stop_mutex;
#endif

/* This list keeps all consumer threads that are running */
static AGIOS_LIST_HEAD(consumers); //for now we only support one consumer 
struct consumer_t consumer;

static int selected_alg = -1;

inline int get_selected_alg()
{
	return selected_alg;
}

int start_consumer(struct client *clnt)
{
	int ret;

	consumer_init();
#ifdef AGIOS_KERNEL_MODULE
	consumer.task = NULL;
#else
	consumer.task = 1;
#endif
	consumer.client = clnt;
	consumer.io_device_id = 0;

	selected_alg = config_get_default_algorithm();
	if(config_get_select_algorithm())
	{
		/*select the scheduling algorithm automatically. First we need to wait for the prediction module to start*/
		agios_wait_predict_init();
		/*ask the prediction module to make this choice*/
		ret = predict_select_best_algorithm();
		if(ret >= 0) //it could return -1 if it is not able to decide*/
		{
			selected_alg = ret;
			agios_print("Automatically selected algorithm %s\n", get_algorithm_name_from_index(selected_alg));
		}
		else
			agios_print("Could not make a decision about scheduling algorithm, using default\n");
		
	}

	ret = initialize_scheduler(selected_alg, &consumer); 

	//get scheduling algorithm's parameters
	set_max_aggregation_size(consumer.io_scheduler->max_aggreg_size);
	set_needs_hashtable(consumer.io_scheduler->needs_hashtable);
	clnt->sync = consumer.io_scheduler->sync;
	proc_set_needs_hashtable(consumer.io_scheduler->needs_hashtable);
	
	if (ret == 1) {
		agios_list_add_tail(&consumer.list, &consumers);

		ret = agios_start_thread(agios_pthread, agios_thread, "agios_thread", (void *) &consumer);
		
                if(ret != 0)
		{
#ifndef AGIOS_KERNEL_MODULE
			consumer.task = 0;
#endif
                        agios_print("Unable to start a thread to agios!\n");
		}
                else
                        ret = 1;

	}

	return ret;
}

#ifdef AGIOS_KERNEL_MODULE
int __init __agios_init(void)
{
	agios_in_use=0;
	agios_print("module is ready. Call agios_init to start using it.");
	return 0;
}

#endif

int agios_init(struct client *clnt, char *config_file)
{
	int ret;
	int file_counter=0;

#ifdef AGIOS_KERNEL_MODULE
	if(agios_in_use)
	{
		agios_print("module already in use, concurrent use is not supported yet.");
		return -EINVAL;
	}
#endif
	PRINT_FUNCTION_NAME;


	/* checks if the client structure is ok */
	if (!clnt || !clnt->process_request) {
		agios_print("Incorrect client structure\n");
		return -EINVAL;
	}

	register_static_io_schedulers();

	if((ret = read_configuration_file(config_file)) != 0)
		return ret;

	//read the access times file
	read_access_times_functions(config_get_access_times_file());

	/*init the memory structures*/
	if ((ret = request_cache_init()))
	{
		return ret;
	}

	proc_stats_init();

	if(config_get_trace())
	{
		if((file_counter = agios_trace_init()) == -1)
		{
			agios_print("Error opening trace file %s.%d.%s!\n", config_get_trace_file_name(1), file_counter, config_get_trace_file_name(2));
			file_counter = 0;
			request_cache_cleanup(); /*clean up cache structures already allocated*/
			proc_stats_exit();
			return ret;
		}
	}

	/*init the prediction module structures*/
	prediction_module_init(file_counter-1); /*we use (file_counter-1) because file_counter includes the current trace file being written. we don't want to open it to read by the prediction thread*/

	/*
	 * Now start consumer threads.
	 */

	ret = start_consumer(clnt);
	if (ret != 1)
	{
		agios_exit();
	}

#ifdef AGIOS_KERNEL_MODULE
	agios_in_use=1;
#endif
	return 0;
}

#ifdef AGIOS_KERNEL_MODULE
void __exit __agios_exit(void)
{
	if(agios_in_use)
		agios_exit();
	agios_print("module is stopping (for real) now.");
}
#endif
void agios_exit(void)
{
	struct consumer_t *consumer;
	PRINT_FUNCTION_NAME;

#ifdef AGIOS_KERNEL_MODULE
	if(!agios_in_use)
		return;
#endif

	/*stop the agios thread*/
#ifdef AGIOS_KERNEL_MODULE
	kthread_stop(agios_pthread);
#else
	stop_the_agios_thread();
#endif

	/*stop the prediction thread*/
	stop_prediction_thr();

	agios_list_for_each_entry(consumer, &consumers, list) {
		if (consumer->task) {
#ifdef AGIOS_KERNEL_MODULE
			complete(&consumer->request_added);
			wait_for_completion(&consumer->exited);
#else
			agios_mutex_lock(&consumer->request_added_mutex);
			agios_cond_signal(&consumer->request_added_cond);
			agios_mutex_unlock(&consumer->request_added_mutex);

			pthread_join(agios_pthread, NULL);
#endif

			if (consumer->io_scheduler->exit)
				consumer->io_scheduler->exit();
		}
	}

	request_cache_cleanup();
	if(config_get_trace())
		agios_trace_close();
	proc_stats_exit();
	agios_print("stopped for this client. It can already be used by calling agios_init\n");
#ifdef AGIOS_KERNEL_MODULE
	agios_in_use = 0;
#endif
}

void agios_wait_predict_init(void)
{
	printf("I am going to wait until the prediction module finished reading traces!\n");
	predict_init_wait();
	printf("It's done, you can start using the library now!\n");
}

#ifdef AGIOS_KERNEL_MODULE
module_init(__agios_init);
module_exit(__agios_exit);
MODULE_LICENSE("GPL");
#endif
