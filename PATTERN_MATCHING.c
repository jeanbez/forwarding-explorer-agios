#include "pattern_tracker.h"
#include "performance.h"
#include "performance_table.h"
#include "ARMED_BANDIT.h"
#include "PATTERN_MATCHING.h"

struct PM_pattern_t *previous_pattern = NULL;
AGIOS_LIST_HEAD(all_observed_patterns);
short int first_performance_measurement;
unsigned int access_pattern_count=0;
//TODO create configuration file parameters: minimum pattern size (patterns shorter than that will be ignored), pattern file name

//TODO we should discard the first performance measurement

//cleanup a PM_pattern_t structure
inline void free_PM_pattern_t(struct PM_pattern_t **pattern)
{
	if((*pattern))
	{
		//free the access pattern description structure
		free_access_pattern_t(&((*pattern)->description));
		//free the performance information structure 
		if(!agios_list_empty(&((*pattern)->performance)))
		{
			struct scheduler_info_t *tmp, *aux=NULL;
			agios_list_for_each_entry(tmp, &(*pattern)->performance, list)
			{
				if(aux)
				{
					agios_list_del(&aux->list);
					free_scheduler_info_t(&aux);
				}
				aux = tmp;
			} 
			if(aux)
			{
				agios_list_del(&aux->list);
				free_scheduler_info_t(&aux);
			}
		}
		//free the probability network 
		if(!agios_list_empty(&((*pattern)->next_patterns)))
		{
			struct next_patterns_element_t *tmp, *aux=NULL;
			agios_list_for_each_entry(tmp, &(*pattern)->next_patterns, list)
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

		free(*pattern);
	}
}

//read information about an access pattern from the file, and its performance measurements (but not the probability network yet)
struct PM_pattern_t *read_access_pattern_from_file(FILE *fd)
{
	int error, i, sched_count, sched_id, measurements, j, this_error;
	//allocate memory
	struct scheduler_info_t *sched_info;
	struct PM_pattern_t *ret = malloc(sizeof(struct PM_pattern_t));
	if(!ret)
		return ret;
	ret->description = malloc(sizeof(struct access_pattern_t));
	if(!ret->description)
	{
		free(ret);
		return NULL;
	}
	
	//description of the access pattern first	
	error = 0;
	error += fread(&(ret->description->reqnb), sizeof(unsigned int), 1, fd);
	error += fread(&(ret->description->total_size), sizeof(unsigned long), 1, fd);
	error += fread(&(ret->description->read_nb), sizeof(unsigned int), 1, fd);
	error += fread(&(ret->description->read_size), sizeof(unsigned long), 1, fd);
	error += fread(&(ret->description->write_nb), sizeof(unsigned int), 1, fd);
	error += fread(&(ret->description->write_size), sizeof(unsigned long), 1, fd);
	error += fread(&(ret->description->filenb), sizeof(unsigned int), 1, fd);
	if(error != 7)
	{
		free_PM_pattern_t(&ret);
		return NULL;
	}
	init_agios_list_head(&ret->description->requests);
	//the time series
	ret->description->time_series = malloc(sizeof(struct pattern_tracker_req_info_t)*(reqnb+1));
	if(!ret->description->time_series)
	{
		free_PM_pattern_t(&ret);
		return NULL;
	}
	error = 0;
	for(i =0; i<reqnb; i++)
	{
		error += fread(&(ret->description->time_series[i].timestamp), sizeof(unsigned long int), 1, fd); 
		error += fread(&(ret->description->time_series[i].offset), sizeof(long long int), 1, fd); 
		init_agios_list_head(&(ret->description->time_series[i].list));
	}
	if(error != reqnb*2)
	{
		free_PM_pattern_t(&ret);
		return NULL;
	}
	ret->description->original_size = ret->description->reqnb;
	ret->description->aggPtSize = NULL;	
	//read performance information for this access pattern
	init_agios_list_head(&(ret->performance));
	error = fread(&sched_count, sizeof(int), 1, fd); //we store a counter of to how many schedulers we have performance masurements
	if(error != 1)
	{
		free_PM_pattern_t(&ret);
		return NULL;
	}
	error = 0;
	for(i=0; i<sched_count; i++)
	{
		//scheduler id
		error += fread(&sched_id, sizeof(int), 1, fd);
		sched_info = malloc(sizeof(struct scheduler_info_t));
		if(!sched_info)
		{
			free_PM_pattern_t(&ret);
			return NULL;
		}
		sched_info->sched = find_io_scheduler(sched_id);
		//measurements
		reset_scheduler_info(sched_info);
		if(!sched_info->bandwidth_measurements)
		{
			free_PM_pattern_t(&ret);
			return NULL;
		}
		error += fread(&measurements, sizeof(int), 1, fd);
		this_error = 0;
		for(j = 0; j < measurements; j++)
		{
			this_error += fread(&(sched_info->bandwidth_measurements[j].timestamp), sizeof(unsigned long int), 1, fd);
			this_error += fread(&(sched_info->bandwidth_measurements[j].bandwidth), sizeof(double), 1, fd);
		}
		if(this_error != measurements*2)
		{
			free_PM_pattern_t(&ret);
			return NULL;
		}
		sched_info->measurements_end = measurements;
		//other information
		error += fread(&sched_info->bandwidth, sizeof(double), 1, fd);
		error += fread(&sched_info->selection_counter, sizeof(int), 1, fd);
		error += fread(&sched_info->probability, sizeof(int), 1, fd);
		//put it in the list
		agios_list_add_tail(&sched_info->list, &ret->performance);
	}
	if(error != 5*sched_count)
	{
		free_PM_pattern_t(&ret);
		return NULL;
	}
	
	//initialize the rest of the structure to fill probability network later
	init_agios_list_head(&(ret->next_patterns));
	ret->all_counters = 0;

	return ret;
}

void read_pattern_matching_file()
{
	FILE *fd;
	int ret, pat_nb, next_pat, this_ret;
	struct PM_pattern_t **patterns;
	struct PM_pattern_t *new;
	struct next_patterns_element_t *tmp;
	int i;

	PRINT_FUNCTION_NAME;
	
	//initialize the list of patterns
	init_agios_list_head(&all_observed_patterns);

	//open file
	fd = fopen(config_pattern_filename, "r");

	//read the number of access patterns
	ret = fread(&access_pattern_count, sizeof(unsigned int), 1, fd);
	if(ret != 1)
	{
		agios_print("PANIC! Could not read from access pattern file %s\n", config_pattern_filename);
		fclose(fd);
		return;	
	}
	//we'll allocate a vector to help us while reading
	patterns = malloc(sizeof(struct PM_pattern_t *)*(access_pattern_count+1));
	if(!patterns)
	{
		agios_print("PANIC! Could not allocate memory for access pattern tracking\n");
		fclose(fd);
		return;
	}

	//first we read the information on the known access patterns
	for(i=0; i< access_pattern_count; i++)
	{
		//get it
		new = read_access_pattern_from_file(fd);
		//store it in our vector
		patterns[i] = new;
		//put it in the list
		agios_list_add_tail(&new->list, &all_observed_patterns);
	}

	//then we read performance information for the known access patterns
	ret = 0;
	agios_list_for_each_entry(new, &all_observed_patterns, list)
	{
		//we store a counter of future patterns
		ret += fread(&pat_nb, sizeof(int), 1, fd);
		this_ret = 0;
		for(i = 0; i< pat_nb; i++)
		{
			//we build a new next pattern data structure
			tmp = malloc(sizeof(struct next_patterns_element_t));
			if(!tmp)
			{
				agios_print("PANIC! Could not allocate memory for access pattern tracking\n");
				fclose(fd);
				free(patterns);
				return;
			}
			//get the identifier of the next pattern and store it
			this_ret += fread(&next_pat, sizeof(int), 1, fd);
			tmp->pattern = patterns[next_pat];
			//get other relevant information
			this_ret += fread(&tmp->counter, sizeof(unsigned int), 1, fd);
			new->all_counters += tmp->counter;
			this_ret += fread(&tmp->probability, sizeof(int), 1, fd);
			//store this information for this access pattern
			agios_list_add_tail(&tmp->list, &new->next_patterns);
		}
		if(this_ret != pat_nb*3)
		{	
			agios_print("PANIC! Could not read pattern matching probability network information\n");
			fclose(fd);
			free(patterns);
			return;
		}
	} //end agios_list_for_each_entry
	if(ret != access_pattern_count)
	{
		agios_print("PANIC! Could not read pattern matching probability network information\n");
		fclose(fd);
		free(patterns);
		return;
	}

	//close file
	fclose(fd);
	//free memory
	free(patterns);
	PRINT_FUNCTION_EXIT;
}

int PATTERN_MATCHING_init(void)
{
	int ret;

	first_performance_measurement = 1;
	//start the pattern tracker
	pattern_tracker_init();

	//this is actually a parameter from configuration file, but it won't work as it should if the user did not set it to 0, so lets make sure
	config_agios_select_algorithm_min_reqnumber = 0;

	//initialize the armed bandit approach so we can use it when we don't know what to do
	ARMED_BANDIT_aux_init();

	//read the pattern matching file we keep between executions so we are always smart
	read_pattern_matching_file();

	return 1;
}
int PATTERN_MATCHING_select_next_algorithm(void)
{
	struct access_pattern_t *seen_pattern;
	struct PM_pattern_t *matched_pattern;
	int new_sched;
	double *recent_measurements;
	short int decided=0;

	//get recent performance measurements
	recent_measurements = agios_get_performance_bandwidth();
	
	//give recent measurements to ARMED BANDIT so it will have updated information
	if(!first_performance_measurement)	
		ARMED_BANDIT_update_bandwidth(recent_measurements,0);
	else
	{
		agios_reset_performance_counters(); //we really discard that first measurement		     
		first_performance_measurement=0;	
	}

	//get the most recently tracked access pattern from the pattern tracker module
	seen_pattern = get_current_pattern(); //TODO remember to free memory used by seen_pattern


	if(match_seen_pattern(seen_pattern)) //if we were able to get a match for this pattern
	{



	
	if(seen_pattern->reqnb >= agios_config_minimum_pattern_size) //if the pattern has too few requests, we'll ignore it (at least for now)
	{
		//we'll look for a match among the patterns we know
			
	}
	else
		decided = 0;

	if(decided == 0)
	{
		//TODO armed bandit will decide what to do
	}	
	else
	{
		//TODO
		//tell ARMED BANDIT which one is the new current scheduler so it will keep updated performance measurements
		ARMED_BANDIT_set_current_sched(new_sched);
	}
	

	//first thing, we need to identify the pattern we've just experienced
	//2 TODO we look for this pattern in the all_observed_patterns list, put the match in seen_pattern
	//2.1 TODO we get a performance measurement and save it for the current scheduling algorithm in seen_pattern
	//3 TODO we look for the seen_pattern in the next patterns from previous_pattern so we can update its counters and probability
	//4 TODO based on the probability chain, we predict the next pattern
	//5 TODO based on the next pattern we're predicting, we choose the next scheduling algorithm
	//6 TODO if we can't make predictions, we'll use the Armed Bandit

	return new_sched;
	
}
void PATTERN_MATCHING_exit(void)
{
	//TODO write file with what we have learned in this execution
}
