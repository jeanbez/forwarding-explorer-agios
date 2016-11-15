#include "performance_table.h"
#include "agios_config.h"

//cleanup a scheduler_info_t structure
inline void free_scheduler_info_t(struct scheduler_info_t **info)
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
inline void reset_scheduler_info(struct scheduler_info_t *info)
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

//updates information about a scheduler with a new performance measurement
inline void add_performance_measurement_to_sched_info(struct scheduler_info_t *info, unsigned long long timestamp, double bandwidth)
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
//checks the performance measurement list for a scheduling algorithm, discarding everything older than the validity window 
//receives as parameters the scheduler info struct to be checked and the most recent timestamp;
//returns 1 if something was discarded, 0 if the list if unchanged
short int check_validity_window(struct scheduler_info_t *info, unsigned long int now)
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
