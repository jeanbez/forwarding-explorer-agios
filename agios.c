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
#include "performance.h"
#include "iosched.h"

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
#endif

//start the scheduling thread
//returns 1 in case of success
int start_consumer(struct client *clnt)
{
	int ret;

#ifdef AGIOS_KERNEL_MODULE
	consumer_init(clnt, NULL);
#else
	consumer_init(clnt, 1);
#endif

	ret = agios_start_thread(agios_pthread, agios_thread, "agios_thread", NULL);
		
        if(ret != 0)
	{
#ifndef AGIOS_KERNEL_MODULE
		consumer_task = 0;
#endif
                agios_print("Unable to start a thread to agios!\n");
	}
	else
		ret = 1;

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

//initializes agios
//returns 0 on success
//if config_file is NULL, the DEFAULT_CONFIGFILE will be read instead. 
//If it does not exist, will use default values
//if requests are not passed to AGIOS with an application id, max_app_id should be 0
int agios_init(struct client *clnt, char *config_file, int max_app_id)
{
	int ret;
	int file_counter=0;

#ifdef AGIOS_KERNEL_MODULE
	if(agios_in_use)
	{
		agios_print("module already in use, concurrent use is not supported.");
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

	if((ret = agios_performance_init()) != 0)
		return ret;

	//read the access times file
	read_access_times_functions(config_agios_access_times_file);

	/*init the memory structures*/
	if ((ret = request_cache_init(max_app_id)))
	{
		return ret;
	}

	proc_stats_init();

	if(config_trace_agios)
	{
		if((file_counter = agios_trace_init()) == -1)
		{
			agios_print("Error opening trace file %s.%d.%s!\n", config_trace_agios_file_prefix, file_counter, config_trace_agios_file_sufix);
			file_counter = 0;
			request_cache_cleanup(); /*clean up cache structures already allocated*/
			proc_stats_exit();
			return ret;
		}
	}

	if(config_predict_agios_read_traces || config_predict_agios_request_aggregation || config_agios_write_simplified_traces) 
	{
		/*init the prediction module structures*/
		prediction_module_init(file_counter-1); /*we use (file_counter-1) because file_counter includes the current trace file being written. we don't want to open it to read by the prediction thread*/
	}

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

	if (consumer_task != 0) 
	{
		consumer_signal_new_reqs();
#ifdef AGIOS_KERNEL_MODULE
		consumer_wait_completion();
#else
		pthread_join(agios_pthread, NULL);
#endif
		if(current_scheduler->exit)
			current_scheduler->exit();
	}

	request_cache_cleanup();
	if(config_trace_agios)
		agios_trace_close();
	proc_stats_exit();
	config_agios_cleanup();
	agios_print("stopped for this client. It can already be used by calling agios_init\n");
#ifdef AGIOS_KERNEL_MODULE
	agios_in_use = 0;
#endif
}

void agios_wait_predict_init(void)
{
	if(config_predict_agios_read_traces || config_predict_agios_request_aggregation || config_agios_write_simplified_traces) 
	{
		printf("I am going to wait until the prediction module finished reading traces!\n");
		predict_init_wait();
		printf("It's done, you can start using the library now!\n");
	}
}

#ifdef AGIOS_KERNEL_MODULE
module_init(__agios_init);
module_exit(__agios_exit);
MODULE_LICENSE("GPL");
#endif
