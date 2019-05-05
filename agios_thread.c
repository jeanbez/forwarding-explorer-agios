/*! \file agios_thread.c
    \brief Implementation of the agios thread.

    The agios thread stays in a loop of calling a scheduler to process new requests and waiting for new requests to arrive.
*/
#include <stdbool.h>
#include <stdtypes.h>
#include <pthread.h>

#include "agios_thread.h"

static pthread_cond_t g_request_added_cond = PTHREAD_COND_INITIALIZER;  /**< Used to let the agios thread know that we have new requests. */
static pthread_mutex_t g_request_added_mutex = PTHREAD_MUTEX_INITIALIZER; /**< Used to protect the request_added_cond. */
static bool g_agios_thread_stop = false; /**< Set to true when the agios_exit function calls stop_the_agios_thread. */
static struct timespec g_last_algorithm_update; //the time at the last time we've selected an algorithm
static struct io_scheduler_instance_t *g_dynamic_scheduler=NULL; /**< The scheduling algorithm chosen in the configuration parameters. */ 

/**
 * function called when a new request is added to wake up the agios thread in case it is sleeping waiting for new requests.
 */
void signal_new_req_to_agios_thread(void)
{
	pthread_mutex_lock(&g_request_added_mutex);
	pthread_cond_signal(&g_request_added_cond);
	pthread_mutex_unlock(&g_request_added_mutex);
}
/**
 * function called by the agios_exit function to let the agios thread know we are finishing the execution.
 */
void stop_the_agios_thread(void)
{
	g_agios_thread_stop = true; //we chose not to protect this variable with a mutex because there is a single thread writing to it and a single thread reading it, the worse it could happen is that the agios thread does not read it correctly, but then it will read it later. It is not a big deal if the agios_exit call waits a little longer.
	//we signal the agios thread so it will wake up if it is sleeping
	signal_new_req_to_agios_thread();
}
/** 
 * the main function executed by the agios thread, which is responsible for processing requests that have been added to AGIOS.
 */
void * agios_thread(void *arg)
{
	struct timespec timeout_tspec; /**< Used to set a timeout for pthread_cond_timedwait, so the thread periodically checks if it has to end. */
	uint32_t current_timeout = config_waiting_time; /**< We have a configuration parameter to define for how long we should sleep when we sleep, but we could decrease this timeout temporarily if we have some periodic thing happening (for instance changing the scheduling algorithm). */
	int32_t remaining_time; /**< Used to calculate how long until we change the scheduling algorithm again */

	//find out which I/O scheduling algorithm we need to use
	g_dynamic_scheduler = initialize_scheduler(config_agios_default_algorithm); //if the scheduler has an init function, it will be called
	//a dynamic scheduling algorithm is a scheduling algorithm that periodically selects other scheduling algorithms to be used
	if (!g_dynamic_scheduler->is_dynamic) { //we are NOT using a dynamic scheduling algorithm
		current_alg = config_agios_default_algorithm;
		current_scheduler = g_dynamic_scheduler;  
	} else { //we ARE using a dynamic scheduler
		//with which algorithm should we start?
		current_alg = config_agios_starting_algorithm; 
		current_scheduler = initialize_scheduler(current_alg);
		agios_gettime(&g_last_algorithm_update);	//we will change the algorithm periodically		
	}
	statistics_set_new_algorithm(current_alg);
	performance_set_new_algorithm(current_alg);
	debug("selected algorithm: %s", current_scheduler->name);
	//since the current algorithm is decided, we can allow requests to be included
	unlock_all_data_structures();
	
	//execution loop, it only stops when we close the library
	do {
		if (0 == get_current_reqnb()) { //here we use the mutex to access the variable current_reqnb because we don't want to risk getting an outdated value and then sleeping for nothing
			/* there are no requests to be processed, so we just have to wait until a new request arrives. 
			 * We use a timeout to avoid a situation where we missed the signal and will sleep forever, and
                         * also because we have to check once in a while to see if we should end the execution.
			 */
			/*! \todo we might want to sleep for other reasons (like when the scheduling algorithms ask for it) */
			if (current_timeout > 0) {
				/*fill the timeout structure*/
				timeout_tspec.tv_sec =  current_timeout / 1000000000L;
				timeout_tspec.tv_nsec = current_timeout % 1000000000L;
	 			pthread_mutex_lock(&g_request_added_mutex);
				pthread_cond_timedwait(&g_request_added_cond, &g_request_added_mutex, &timeout_tspec);
				pthread_mutex_unlock(&g_request_added_mutex);
			}
			current_timeout = config_waiting_time; //the timeout is only changed temporarily (to a single call of cond_timedwait) */
		}
		//we got past the above loop, so we could have requests to process, or reached the timeout of sleeping, or got a signal to finish the execution
		//check if we should finish the execution
		if (!g_agios_thread_stop) {
			//check if it is time to change the scheduling algorithm
			if (g_dynamic_scheduler->is_dynamic) {
				remaining_time = get_nanoelapsed(g_last_algorithm_update) - config_agios_select_algorithm_period;
				if (remaining_time <= 0) { //it is time to select!
					//make a decision on the next scheduling algorithm
					next_alg = d_dynamic_scheduler->select_algorithm();
					//change it
					debug("HEY IM CHANGING THE SCHEDULING ALGORITHM\n\n\n\n");
					change_selected_alg(next_alg);
					statistics_set_new_algorithm(current_alg);
					performance_set_new_algorithm(current_alg);
					reset_stats_window(); //reset all stats so they will not affect the next selection
					unlock_all_data_structures(); //we can allow new requests to be added now
					agios_gettime(&g_last_algorithm_update); 
					debug("We've changed the scheduling algorithm to %s", current_scheduler->name);
					if (config_agios_select_algorithm_period < config_waiting_time) current_timeout = config_agios_select_algorithm_period;
				} else { //it is NOT time to select
					if (remaining_time < current_timeout) current_timeout = remaining_time;
				}
			} //end scheduler is dynamic
			//try to process requests
			remaining_time = current_scheduler->schedule(client); //the scheduler may have a reason to ask us for a shorter timeout (for instance, TWINS keeps track of time windows) 
			//TODO adapt the scheduling algorithms
			if (remaining_time < current_timeout) current_timeout = remaining_time;
		} //end if(!g_agios_thread_stop)
        } while (!g_agios_thread_stop);

	return 0;
}
//TODO dont change the scheduling algorithm if we have not processed a certain number of requests (config_min... )
