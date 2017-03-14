/* File:	performance_table.c
 * Created: 	2016 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides a structure so the pattern matching appraoch and the armed bandit
 *		algorithm may keep performance information.
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

#include <stdlib.h>
#include "performance_table.h"
#include "agios_config.h"
#include "common_functions.h"

//cleanup a scheduler_info_t structure
void free_scheduler_info_t(struct scheduler_info_t **info)
{
	if(*info)
	{
		//we don't need to free the sched structure, since it was not allocated (we just got the pointer)
		if((*info)->bandwidth_measurements)
			free((*info)->bandwidth_measurements);
		free(*info);
	}
}

//resets all information from a scheduler_info_t struct, except the "sched" field
void reset_scheduler_info(struct scheduler_info_t *info)
{
	info->bandwidth = 0.0;
	info->selection_counter=0;
	info->probability = 0;
	info->bandwidth_measurements = (struct performance_info_t *)malloc(sizeof(struct performance_info_t)*config_agios_performance_window);
	if(!info->bandwidth_measurements)
	{
		agios_print("PANIC! Could not allocate memory for performance table\n");
		//now what?
	}
	info->measurements_start = info->measurements_end = 0;
}

//gets the number of schedulers to which we have performance measurements in a performance table (the size of the list)
int get_sched_info_number(struct agios_list_head *table)
{
	int ret =0;
	struct scheduler_info_t *tmp;

	if(!agios_list_empty(table))
	{
		agios_list_for_each_entry(tmp, table, list)
		{
			ret++;
		}
	}
	return ret;
}
//gets the number of measurements in a scheduler_info-t structure (they are stored in a circular list)
int get_performance_measurements_number(struct scheduler_info_t *info)
{
	int start = info->measurements_start;
	int ret=0;
	while(start != info->measurements_end)
	{
		ret++;
		start++;
		if(start == config_agios_performance_window)
			start = 0;
	}
	return ret;
}

//updates information about a scheduler with a new performance measurement
void add_performance_measurement_to_sched_info(struct scheduler_info_t *info, long long timestamp, double bandwidth)
{
	info->bandwidth_measurements[info->measurements_end].timestamp = timestamp;
	info->bandwidth_measurements[info->measurements_end].bandwidth = bandwidth;
	info->measurements_end++;
	if(info->measurements_end == config_agios_performance_window)
	{
		info->measurements_end=0;
	}
	if(info->measurements_end == info->measurements_start)
	{
		info->measurements_start++;
		if(info->measurements_start == config_agios_performance_window)
			info->measurements_start=0;
	}
}
void add_measurement_to_performance_table(struct agios_list_head *table, int current_sched, long long timestamp, double bandwidth)
{
	struct scheduler_info_t *tmp;
	short int found=0;
	//look for the line corresponding to this scheduler
	agios_list_for_each_entry(tmp, table, list)
	{
		if(tmp->sched->index == current_sched)
		{
			found=1;
			break;
		}
	}
	//if we can't find, we need to include a new one
	if(!found)
	{
		tmp = malloc(sizeof(struct scheduler_info_t));
		if(!tmp)
		{	
			agios_print("PANIC! Could not allocate memory for performance table\n");
			return;
		}
		reset_scheduler_info(tmp);
		tmp->sched = find_io_scheduler(current_sched);
		agios_list_add_tail(&tmp->list, table);
	}
	//now we have the line, so we add the new information
	add_performance_measurement_to_sched_info(tmp, timestamp, bandwidth);
	check_validity_window(tmp, timestamp);
	update_average_bandwidth(tmp);
	
}
//checks the performance measurement list for a scheduling algorithm, discarding everything older than the validity window 
//receives as parameters the scheduler info struct to be checked and the most recent timestamp;
//returns 1 if something was discarded, 0 if the list if unchanged
short int check_validity_window(struct scheduler_info_t *info, long int now)
{
	short int changed=0;

	while((info->measurements_start != info->measurements_end) //while we have measurements in the list
	      &&
	      ((now - info->bandwidth_measurements[info->measurements_start].timestamp) >= config_agios_validity_window))
	{
		info->measurements_start++; //discard this measurement, it's too old
		if(info->measurements_start == config_agios_performance_window)
			info->measurements_start = 0;
		changed=1;
	}
	return changed;
}

//updates bandwidth information for a scheduling algorithm by taking the average of its measurements
//this function does not check the validity window, this needs to be done before calling it.
void update_average_bandwidth(struct scheduler_info_t *info)
{
	int i;
	long double ret=0.0;
	int counter=0;

	i = info->measurements_start;
	while(i != info->measurements_end)
	{
		ret += info->bandwidth_measurements[i].bandwidth;
		counter++; 	
		i++;
		if(i == config_agios_performance_window)
			i = 0;
	}
	if(counter > 0) //we could have an empty performance measurement list
		info->bandwidth = ret / counter;
	else
		info->bandwidth = 0.0;
}

//TODO a function to calculate the median bandwidth (instead of the average)
