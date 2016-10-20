#include "agios.h"
#include "agios_request.h"
#include "common_functions.h"

static pthread_t pattern_tracker_thread;

//to keep interaction with the pattern tracker thread safe
pthread_mutex_t pattern_tracker_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pattern_tracker_cond = PTHREAD_COND_INITIALIZER;
short int pattern_tracker_stop = 0;

//current access pattern
struct pattern_tracker_req_info_t
{
	unsigned long int timestamp;
	unsigned long int offset;
	unsigned long int len;
	//TODO see with Ramon what we need to track	
	struct agios_list_head list;
}
int pattern_tracker_reqnb = 0;
AGIOS_LIST_HEAD(tracked_pattern);

//are we pattern tracking?
short int agios_is_pattern_tracking = 0;

//stops the pattern tracker thread 
inline void stop_the_pattern_tracker_thread()
{
	if(agios_is_pattern_tracking)
	{
		agios_mutex_lock(&pattern_tracker_lock);
		pattern_tracker_stop = 1;
		agios_cond_signal(&pattern_tracker_cond);
		agios_mutex_unlock(&pattern_tracker_lock);
		//wait for it
		pthread_join(pattern_tracker_thread, NULL);
	}
}

//adds information about a new request to the tracked pattern
inline void add_request_to_pattern(unsigned long int timestamp, unsigned long int offset, unsigned long int len)
{
	if(agios_is_pattern_tracking)
	{
		//allocate structure and fill it
		struct pattern_tracker_req_info_t *new = agios_alloc(sizeof(struct pattern_tracker_req_info_t));
		init_agios_list_head(&new->list);
		new->timestamp = timestamp;
		new->offset = offset;
		new->len = len;
		//TODO what more should we get from requests?

		//put the new structure in the pattern
		agios_mutex_lock(&pattern_tracker_lock);
		agios_list_add_tail(&new->list, &tracked_pattern);
		pattern_tracker_reqnb++;
		agios_mutex_unlock(&pattern_tracker_lock);
	
		//we will NOT notify the pattern tracker thread because we just want to finish this function as quickly as possible, the pattern tracker will notice eventually there are queued requests
	}
}

//function executed by the pattern tracker thread. It will keep running until notified by the stop_the_pattern_tracker_thread function is called.
void *pattern_tracker_run(void *arg)
{
	PRINT_FUNCTION_NAME;
	unsigned long int pattern_start = 0;
	struct timespec pattern_start_tspec, timeout_tspec;
	unsigned long int timeout;
	struct pattern_tracker_req_info_t *tmp, *rmv;

	//run until we are told to stop
	do {
		if(pattern_start == 0) //we still did not receive the first request, so we are not counting for a pattern
			get_llu2timespec(agios_config_pattern_duration, &timeout_tspec);
		else
		{
			// we set a timeout to the end of the pattern duration
			timeout = agios_config_pattern_duration - get_nanoelapsed_llu(pattern_start);
			get_llu2timespec(timeout, &timeout_tspec);
		}

		//now we wait for the end of this pattern
		agios_mutex_lock(&pattern_tracker_lock);

		pthread_cond_timedwait(&pattern_tracker_cond, &pattern_tracker_lock, &timeout_tspec);
		//reset time counter  (for the next pattern)
		agios_gettime(&timeout_tspec);
		pattern_start = get_timespec2llu(timeout_tspec); 			

		agios_mutex_unlock(&pattern_tracker_lock);
		
		//ok, so if we are here we have been notified or timed out
		if(!pattern_tracker_stop) //were we notified because it is time to stop?
		{
			//if not, then we timedout
			if(pattern_tracker_reqnb > 0) //and we have a pattern
			{
				//TODO do something about the current pattern
			}
		}
	} while(!pattern_tracker_stop)
}

//starts the pattern tracker thread, returns 0 in success
int pattern_tracker_init()
{
	int ret=0;

	//initialize the queue
	init_agios_list_head(&tracked_pattern);

	//create the thread
	ret = agios_start_thread(pattern_tracker_thread, pattern_tracker_run, "pattern_tracker_thread", NULL);
	if (ret != 0)
		agios_print("Unable to start a thread for the pattern tracker\n")
	return ret;
}
//TODO we need to call init from somewhere!
