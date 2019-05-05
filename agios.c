/*! \file agios.c
    \brief Implementation of the agios_init and agios_exit functions, used to start and end the library.

    To use AGIOS, the user needs to call agios_init and ensure it returned sucess. Then requests are given to AGIOS with agios_add_request. When the scheduler decides it is time to process a request, it will use the callback functions provided to agios_init.
*/
#include <stdbool.h>
#include <stdtypes.h>

#include "agios.h"

static pthread_t g_agios_thread; /**< AGIOS thread that will run the AGIOS_thread function.  */

/**
 * function used by agios_exit and agios_init (in case of errors) to clean up all allocated memory.
 */
void cleanup_agios(void)
{
	cleanup_config_parameters();
	cleanup_performance_module();
	cleanup_data_structures();
	cleanup_statistics_module();
	if (config_trace_agios) {
		close_agios_trace();
		cleanup_agios_trace();
	}
}

/**
 * function called by the user to initialize AGIOS. It will read parameters, allocate memory and start the AGIOS thread.
 * @param process_request the callback function from the user code used by AGIOS to process a single request. (required)
 * @param process_requests the callback function from the user code used by AGIOS to process a list of requests. (optional)
 * @param config_file the path to a configuration file. If NULL, the DEFAULT_CONFIGFILE will be read instead. If the default configuration file does not exist, it will use default values.
 * @param max_queue_id for schedulers that use multiple queues, one per server/application (TWINS and SW), define the number of queues to be used. If it is not relevant to the used scheduler, it is better to provide 0. With each request being added, a value between 0 and max_queue_id-1 is to be provided.
 * TODO link with @see to configuration parameters
 * @return true of false for success.
 */
bool agios_init(void * process_request(int64_t req_id), 
		void * process_requests(int64_t *reqs, int32_t reqnb), 
		char *config_file, 
		int32_t max_queue_id)
{
	//check if a callback was provided 
	if (!process_request) {
		agios_print("Incorrect parameters to agios_init\n");
		return false; //we don't use the goto cleanup_on_error because we have nothing to clean up
	}
	if (!read_configuration_file(config_file)) goto cleanup_on_error; 
	if (!init_performance_module()) goto cleanup_on_error;
	if (!allocate_data_structures(max_app_id)) goto cleanup_on_error;
	if (!init_statistics_module()) goto cleanup_on_error;
	//if we are going to generate traces, init the tracing module
	if (config_trace_agios) {
		if (!init_trace_module()) goto cleanup_on_error;
	}
	//init the AGIOS thread
	int32_t ret = pthread_create(&g_agios_thread, NULL, agios_thread, NULL);
	if (ret != 0) {
                agios_print("Unable to start a thread to agios!\n");
		goto cleanup_on_error;
	}
	//success, finish the function call
	return true;
cleanup_on_error:  //used to abort the initialization if anything goes wrong
	cleanup_agios();
	return false;
}

/**
 * function called by the user to stop AGIOS. It will stop the AGIOS thread and free all allocated memory.
 */
void agios_exit(void)
{
	//stop the agios thread
	stop_the_agios_thread();
	pthread_join(g_agios_thread, NULL);
	if (current_scheduler->exit) current_scheduler->exit(); //the exit function is not mandatory for schedulers
	//cleanup memory
	cleanup_agios();
	agios_print("stopped for this client. AGIOS can be used again by calling agios_init\n");
}
