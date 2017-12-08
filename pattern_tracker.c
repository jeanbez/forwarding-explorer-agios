/* File:	pattern_tracker.c
 * Created: 	2016
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It tracks accesses for the pattern matching approach implementation
 *		Further information is available at http://inf.ufrgs.br/~fzboito/agios.html
 *
 * Contributors:
 *		Federal University of Rio Grande do Sul (UFRGS)
 *		INRIA France
 *
 *		This program is distributed in the hope that it will be useful,
 * 		but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <string.h>
#include <limits.h>

#include "agios.h"
#include "agios_request.h"
#include "common_functions.h"
#include "mylist.h"
#include "pattern_tracker.h"

//to access the tracked_pattern list
pthread_mutex_t pattern_tracker_lock = PTHREAD_MUTEX_INITIALIZER;

//current access pattern
struct access_pattern_t *current_pattern;
long int last_offset=0;

//files being accessed right now
char * file_ids[MAXIMUM_FILE_NUMBER];

//are we pattern tracking?
short int agios_is_pattern_tracking = 0;

//adds information about a new request to the tracked pattern
void add_request_to_pattern(long int timestamp, long int offset, long int len, short int type, char *file_id)
{
	if(agios_is_pattern_tracking) //so if the flag is not set (only the pattern matching algorithm sets it), this function will do nothing
	{
		int i;
		long int calculated_offset;

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
		calculated_offset = i*MAXIMUM_FILE_SIZE + (offset/1024); //we keep offsets in KB
		new->offset = calculated_offset - last_offset; //the offset difference, actually
		last_offset = calculated_offset;


		//put the new structure in the pattern
		agios_list_add_tail(&new->list, &current_pattern->requests);

		agios_mutex_unlock(&pattern_tracker_lock);
	}
}

//modify the current_pattern structure so the linked list of requests we were keeping while execution becomes an array
void translate_list_to_time_series()
{
	struct pattern_tracker_req_info_t *tmp, *aux=NULL;
	int i=0;

	current_pattern->time_series = malloc(sizeof(struct pattern_tracker_req_info_t)*(current_pattern->reqnb+1));
	if(!current_pattern->time_series)
	{
		agios_print("PANIC! Could not allocate memory for access pattern tracking\n");
		return;
	}

	agios_list_for_each_entry(tmp, &current_pattern->requests, list)
	{
		if(aux)
		{
			agios_list_del(&aux->list);
			free(aux);
		}
		current_pattern->time_series[i].offset = tmp->offset;
		i++;
		aux = tmp;
	}
	if(aux)
	{
		agios_list_del(&aux->list);
		free(aux);
	}
}

//returns the current pattern and resets it so we will start to track the next pattern
struct access_pattern_t *get_current_pattern()
{
	struct access_pattern_t *ret;

	agios_mutex_lock(&pattern_tracker_lock);

	translate_list_to_time_series();
	ret = current_pattern;
	reset_current_pattern(0);

	agios_mutex_unlock(&pattern_tracker_lock);
	return ret;
}


void reset_current_pattern(short int first_execution)
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
	current_pattern->aggPtSize=NULL;
	current_pattern->time_series=NULL;

	//TODO if we are keeping the file handle to starting offset mapping through different executions, then we won't never reset this. Actually, we should start by reading it from some file and end by writing its new version.
	for(i = 0; i <MAXIMUM_FILE_NUMBER; i++)
	{
		if(!first_execution) //in the first execution we simply set them all to NULL, but on future calls of this function, we may have some memory to free
			if(file_ids[i])
				free(file_ids[i]);
		file_ids[i] = NULL;
	}

	last_offset=0;
}

//init the pattern_tracker (nothing revolutionary here)
void pattern_tracker_init()
{
	agios_is_pattern_tracking=1;
	//initialize the current access pattern
	reset_current_pattern(1);
}

//frees an access_pattern_t structure previously allocated
void free_access_pattern_t(struct access_pattern_t **ap)
{
	if(*ap)
	{
		if(!agios_list_empty(&((*ap)->requests)))
		{
			struct pattern_tracker_req_info_t *tmp, *aux=NULL;
			agios_list_for_each_entry(tmp, &((*ap)->requests), list)
			{
				if(aux)
				{
					agios_list_del(&aux->list);
					free(aux);
				}
				aux = tmp;
			}
			if(aux)
			{
				agios_list_del(&aux->list);
				free(aux);
			}
		}
		if((*ap)->time_series)
			free((*ap)->time_series);
		if((*ap)->aggPtSize)
			free((*ap)->aggPtSize);
		free(*ap);
	}

}
