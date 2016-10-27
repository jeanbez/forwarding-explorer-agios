#include <string.h>

#include "agios.h"
#include "agios_request.h"
#include "common_functions.h"
#include "mylist.h"
#include "pattern_tracker.h"

//to access the tracked_pattern list
pthread_mutex_t pattern_tracker_lock = PTHREAD_MUTEX_INITIALIZER;

//current access pattern
struct access_pattern_t *current_pattern;

//files being accessed right now
char * file_ids[MAXIMUM_FILE_NUMBER];

//are we pattern tracking?
short int agios_is_pattern_tracking = 0;

//adds information about a new request to the tracked pattern
inline void add_request_to_pattern(unsigned long int timestamp, unsigned long int offset, unsigned long int len, short int type, char *file_id)
{
	if(agios_is_pattern_tracking) //so if the flag is not set (only the pattern matching algorithm sets it), this function will do nothing
	{
		int i;

		agios_mutex_lock(&pattern_tracker_lock);

		//update counters
		current_pattern->reqnb++;
		current_pattern->total_size+=len;
		if(type == RT_READ)
		{
			current_pattern->read_nb++;
			current_pattern->read_size+=len;
		}
		else
		{
			current_pattern->write_nb++;
			current_pattern->write_size +=len;
		}

		//register and get starting offset for the currently accessed file
		for(i = 0; i< MAXIMUM_FILE_NUMBER; i++)
		{
			if(!file_ids[i])
				break;  //we finished going through all registered files and did not find this one
			if(strcmp(file_ids[i], file_id) == 0)
				break;
		}
		if(!file_ids[i]) //we need to add this file
		{
			current_pattern->filenb++;
			file_ids[i] = malloc((strlen(file_id)+1)*sizeof(char));
			strcpy(file_ids[i], file_id);
		}
		//i is the index of the file accessed by this request
		
		
		//allocate structure and fill it
		struct pattern_tracker_req_info_t *new = agios_alloc(sizeof(struct pattern_tracker_req_info_t));
		init_agios_list_head(&new->list);
		new->timestamp = timestamp;
		new->offset = i*MAXIMUM_FILE_SIZE + offset;

		//put the new structure in the pattern
		agios_list_add_tail(&new->list, &current_pattern->requests);

		agios_mutex_unlock(&pattern_tracker_lock);
	}
}

//returns the current pattern and resets it so we will start to track the next pattern
struct access_pattern_t *get_pattern()
{
	struct access_pattern_t *ret = current_pattern;
	reset_current_pattern(0);
	return ret;
}

inline void reset_current_pattern(short int first_execution)
{
	int i;

	current_pattern = malloc(sizeof(struct access_pattern_t));
	if(!current_pattern)
	{
		agios_print("PANIC! Could not allocate memory for access pattern tracking!\n");
		return;
	}
	current_pattern->reqnb = 0;
	current_pattern->total_size = 0;
	current_pattern->read_nb = 0;
	current_pattern->read_size = 0;
	current_pattern->write_nb = 0;
	current_pattern->write_size = 0;
	current_pattern->filenb = 0;
	init_agios_list_head(&current_pattern->requests);

	//TODO if we are keeping the file handle to starting offset mapping through different executions, then we won't never reset this. Actually, we should start by reading it from some file and end by writing its new version.
	for(i = 0; i <MAXIMUM_FILE_NUMBER; i++)
	{
		if(!first_execution) //in the first execution we simply set them all to NULL, but on future calls of this function, we may have some memory to free
			if(file_ids[i])
				free(file_ids[i]);
		file_ids[i] = NULL;
	}
}

//init the pattern_tracker (nothing revolutionary here)
void pattern_tracker_init()
{
	agios_is_pattern_tracking=1;
	//initialize the current access pattern
	reset_current_pattern(1);
}
