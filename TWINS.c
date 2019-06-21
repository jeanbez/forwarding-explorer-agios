/*! \file TWINS.c
    \brief Implements the TWINS scheduling algorithm

 */ 
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <string.h>

static bool g_twins_first_req; /**< used to know when twins is being used for the first time (so we'll reset it) */
static int g_current_twins_server; /**< the current queue from where we are taking requests */
static struct timespec g_window_start; /**< the timestamp for the start of the current window */

/**
 * function called to initialize TWINS by setting some variables.
 * @return true or false for success
 */
bool TWINS_init()
{
	g_twins_first_req = true; //we'll start the first time window only when the first request is selected for processing (in the future we might want to reset it every once in a while)
	g_current_twins_server = 0; //the first id we will prioritize
	return true;
}
/**
 * function called when stopping the use of TWINS, for now we don't have anything to clean up.
 */
void TWINS_exit()
{
}
/**
 * main function for the TWINS scheduler. It is called by the AGIOS thread to schedule some requests. It will continue to consume requests until there are no more requests or if notified by the process_requests function.
 * @return time until the end of the current window
 */
int64_t TWINS(void)
{
	bool TWINS_stop=false; /**< the return of the process_requests function may notify us it is time to stop because of a periodic event */
	struct request_t *req; /**< used to access requests from the queues */
	int32_t hash; /**< after selecting a request to be processed, we need to find out its hash to give to the process_requests function */
	
	PRINT_FUNCTION_NAME;
	//we are not locking the current_reqnb_mutex, so we could be using outdated information. We have chosen to do this for performance reasons
	while ((current_reqnb > 0) && (!TWINS_stop)) {
		timeline_lock();
		//do we need to setup the window, or did it end already?
		if (g_twins_first_req) {
			//we are going to start the first window!
			agios_gettime(&g_window_start);
			g_twins_first_req = false;
			g_current_twins_server = 0;
		} else if (get_nanoelapsed(g_window_start) >= config_twins_window) {
			//we're done with this window, time to move to the next one
			agios_gettime(&g_window_start);
			g_current_twins_server++;
			if (g_current_twins_server >= app_timeline_size) g_current_twins_server = 0; //round robin!
			debug("time is up, moving on to window %d", g_current_twins_server);
		}
		//process requests!
		if (!(agios_list_empty(&(app_timeline[g_current_twins_server])))) { //we can only process requests from the current app_id
			//take request from the right queue
			req = agios_list_entry(app_timeline[g_current_twins_server].next, struct request_t, related);
			//remove from the queue
			agios_list_del(&req->related);
			/*send it back to the file system*/
			//we need the hash for this request's file id so we can update its stats 
			hash = get_hashtable_position(req->file_id);
			TWINS_stop = process_requests(req, (struct client *)clnt, hash);
		} else { //if there are no requests for this queue, we return control to the AGIOS thread and it will sleep a little 
			timeline_unlock();
			break; 
		}
		timeline_unlock();
	} //end while
	return (config_twins_window - get_nanoelapsed(g_window_start));
}
