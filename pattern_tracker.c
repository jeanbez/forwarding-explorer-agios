#include "agios.h"
#include "agios_request.h"
#include "common_functions.h"

//to access the tracked_pattern list
pthread_mutex_t pattern_tracker_lock = PTHREAD_MUTEX_INITIALIZER;

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

//adds information about a new request to the tracked pattern
inline void add_request_to_pattern(unsigned long int timestamp, unsigned long int offset, unsigned long int len)
{
	if(agios_is_pattern_tracking) //so if the flag is not set (only the pattern matching algorithm sets it), this function will do nothing
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
	}
}

//init the pattern_tracker (nothing revolutionary here)
void pattern_tracker_init()
{
	//initialize the queue
	init_agios_list_head(&tracked_pattern);
	agios_is_pattern_tracking=1;
}
