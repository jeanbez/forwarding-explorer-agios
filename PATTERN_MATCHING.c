#include "pattern_tracker.h"
#include "performance.h"
#include "performance_table.h"
#include "ARMED_BANDIT.h"
#include "PATTERN_MATCHING.h"

struct PM_pattern_t *previous_pattern = NULL;
AGIOS_LIST_HEAD(all_observed_patterns);
short int first_performance_measurement;
unsigned int access_pattern_count=0;
//TODO create configuration file parameters: minimum pattern size (patterns shorter than that will be ignored), pattern file name, config_pattern_matching_threshold, config_maximum_pattern_difference, config_minimum_pattern_size

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
	for(i =0; i<ret->description->reqnb; i++)
	{
		error += fread(&(ret->description->time_series[i].timestamp), sizeof(unsigned long int), 1, fd); 
		error += fread(&(ret->description->time_series[i].offset), sizeof(long long int), 1, fd); 
		init_agios_list_head(&(ret->description->time_series[i].list));
	}
	if(error != ret->description->reqnb*2)
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
	if(!fd)
	{
		agios_print("Could not open pattern matching file %s\n", config_pattern_filename);
		return;
	}

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
		new->id = i;
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

//apply some heuristics to decide if two access patterns are close enough to compare their time series
inline short int compatible_pattern(struct access_pattern_t *A, struct access_pattern_t *B)
{
	unsigned long diff;

	//let's see the difference in the total number of requests
	diff = ((A->reqnb - B->reqnb)*100)/A->reqnb;
	if(diff < 0)
		diff = 0 - diff;
	if(diff > config_maximum_pattern_difference)
		return 0;
	//let`s see the difference in the number of reads
	diff = ((A->read_nb - B->read_nb)*100)/A->read_nb;
	if(diff < 0)
		diff = 0 - diff;
	if(diff > config_maximum_pattern_difference)
		return 0;
	//let's see the difference in the number of writes
	diff = ((A->write_nb - B->write_nb)*100)/A->write_nb;
	if(diff < 0)
		diff = 0 - diff;
	if(diff > config_maximum_pattern_difference)
		return 0;
	//let's see the difference in the number of accessed files
	diff = ((A->filenb - B->filenb)*100)/A->filenb;
	if(diff < 0)
		diff = 0 - diff;
	if(diff > config_maximum_pattern_difference)
		return 0;
	//ok, they passed all our tests, so we say they are compatible	
	//TODO more heuristics?
	return 1;
}

//look for an access pattern in the list of patterns we know
//return NULL if we can't find a match
struct PM_pattern_t *match_seen_pattern(struct access_pattern_t *pattern)
{
	struct PM_pattern_t *tmp, *ret = NULL;
	double best_result = 0.0;
	double this_result;

	agios_list_for_each_entry(tmp, &all_observed_patterns, list)
	{
		//compare the patterns
	 	if(compatible_pattern(tmp->description, pattern)) //first we apply some heuristics so we won't compare all patterns
		{
			this_result = FastDTW(tmp->description, pattern);
			//among the similar ones, we get the most similar one
			if((this_result >= config_pattern_matching_threshold) && (this_result > best_result)) //TODO we need to check what is the range of results given by FastDTW (to figure out the range of values to give the threshold
			{
				best_result = this_result;
				ret = tmp;
			}
		}
	}
	return ret; 
}
//store a new access pattern in the list of known access patterns
//returns a pointer to it
struct PM_pattern_t *store_new_access_pattern(struct access_pattern_t *patter)
{
	//allocate new data structure
	struct PM_pattern_t *ret = malloc(sizeof(struct PM_pattern_t));
	if(!ret)
	{
		agios_print("PANIC! Could not allocate memory for pattern matching!\n");
		return NULL;
	}
	//initialize the data structure
	ret->description = pattern;
	init_agios_list_head(&ret->performance);
	init_agios_list_head(&ret->next_patterns);
	ret->all_counters = 0;
	//store it
	agios_list_add_tail(&ret->list, &all_observed_patterns);
	ret->id = access_pattern_count;
	access_pattern_count++;
	return ret;
}

void update_probability_network(struct PM_pattern_t *new_pattern)
{
	struct next_patterns_element_t *tmp;
	short int found=0;

	//look for this pattern in the next patterns of the previous pattern
	if(!agios_list_empty(&(previous_pattern->next_patterns)))
	{
		agios_list_for_each_entry(tmp, &(previous_pattern->next_patterns), list)
		{
			if(tmp->pattern == new_pattern)
			{
				found =1;
				break;
			}
		}
	}	
	//if we did not find it, we need to add a new data structure
	tmp = malloc(sizeof(struct next_patterns_element_t));
	if(!tmp)
	{
		agios_print("PANIC! Could not allocate memory for pattern matching\n");
		return;
	}
	tmp->counter = 0;
	tmp->pattern = new_pattern;
	agios_list_add_tail(&tmp->list, &previous_pattern->next_patterns);
	//we count this occurrence now
	tmp->counter++;
	previous_pattern->all_counters++; //we use a lazy approach to update probabilities (we only calculate them when writing the file or making a decision)
}
//look at the probability network from previous_pattern and returns the most likely pattern
struct PM_pattern_t *predict_next_pattern(void)
{
	//we recalculate probabilities and get the highest one
	int best_prob = 0;
	struct next_patterns_element_t *tmp;
	struct PM_pattern_t *ret=NULL;
	if(!agios_list_empty(&(previous_pattern->next_patterns)))
	{
		agios_list_for_each_entry(tmp, &previous_pattern->next_patterns, list)
		{
			tmp->probability = (tmp->counter*100)/previous_pattern->all_counters;
			if(tmp->probability == 0)
				tmp->probability = 1;
			if(tmp->probability > best_prob)
			{
				best_prob = tmp->probability;
				ret = tmp->pattern;		
			}
		}
	}
	return ret;
}
struct io_scheduler_instance_t *get_best_scheduler_for_pattern(struct agios_list_head *table)
{
	struct io_scheduler_instance_t *ret = NULL;
	struct scheduler_info_t *tmp;
	double best_performance=0;

	if(!agios_list_head(table))
	{
		agios_list_for_each_entry(tmp, table, list)
		{
			if(tmp->bandwidth > best_performance)
			{
				best_performance = tmp->bandwidth;
				ret = tmp->sched;
			}
		}
	}
	return ret;
}


int PATTERN_MATCHING_select_next_algorithm(void)
{
	struct access_pattern_t *seen_pattern;
	struct PM_pattern_t *matched_pattern=NULL;
	struct io_scheduler_instance_t *new_sched=NULL;
	double *recent_measurements;
	short int decided=0;
	struct timespec this_time;
	unsigned long long timestamp;

	PRINT_FUNCTION_NAME;

	//get recent performance measurements
	agios_gettime(&this_time);
	timestamp = get_timespec2llu(this_time);
	recent_measurements = agios_get_performance_bandwidth();
	
	//give recent measurements to ARMED BANDIT so it will have updated information
	if(!first_performance_measurement)	
		ARMED_BANDIT_update_bandwidth(recent_measurements,0);
	else
	{
		agios_reset_performance_counters(); //we really discard that first measurement		     
		free(recent_measurements);
		recent_measurements=NULL;
		first_performance_measurement=0;	
	}

	//get the most recently tracked access pattern from the pattern tracker module
	seen_pattern = get_current_pattern(); 

	//look for the recently observed pattern in our list of known patterns
	if(seen_pattern->reqnb >= config_minimum_pattern_size) //we ignore patterns which are too small
	{
		matched_pattern = match_seen_pattern(seen_pattern);
		if(matched_pattern) //we don't need the seen_pattern structure anymore, we can free it
			free_access_pattern_t(&seen_pattern);
		else	//if we did not find the pattern, we'll include it as a new one
			matched_pattern = store_new_access_pattern(seen_pattern);
	}
	else //the pattern is too short, we'll drop it
		free_access_pattern_t(&seen_pattern);

	//if we know this pattern, we store the performance information (if we have the information, because we might have dropped it if it was the first performance measurement)
	if((matched_pattern) && (recent_measurements))
	{
		add_measurement_to_performance_table(&matched_pattern->performance, current_sched, timestamp, recent_measurements[agios_performance_get_latest_index()]); 
	}
	if(recent_measurements) //we may cleanup now, we already got what we needed
		free(recent_measurements);
	
	//we link the previous pattern to this one we've just detected
	if(matched_pattern)
	{
		if(previous_pattern)
			update_probability_network(matched_pattern);
		previous_pattern = matched_pattern;
		//now we can try to predict the next pattern we'll see
		matched_pattern = predict_next_pattern();
	}

	//if we found it, then we can try selecting the best algorithm for the situation
	if(matched_pattern)
	{
		new_sched = get_best_scheduler_for_pattern(&matched_pattern->performance);	
	}

	//if we were able to select a scheduling algorithm, we just have to return, otherwise we'll let armed bandit make the choice for us
	if(new_sched)
	{
		//tell ARMED BANDIT which one is the new current scheduler so it will keep updated performance measurements
		ARMED_BANDIT_set_current_sched(new_sched->index);
		return new_sched->index;
	}
	else
	{
		//if we can't make predictions, we'll use the Armed Bandit
		return ARMED_BANDIT_aux_select_next_algorithm(timestamp);
	}
}
void write_access_pattern_to_file(struct PM_pattern_t *pattern, FILE *fd)
{
	int error=0;
	int i, sched_count, measurements, start;
	struct scheduler_info_t *tmp;

	//description of the access pattern
	error+= fwrite(&(pattern->description->reqnb), sizeof(unsigned int), 1, fd);
	error+= fwrite(&(pattern->description->total_size), sizeof(unsigned long int), 1, fd);
	error+= fwrite(&(pattern->description->read_nb), sizeof(unsigned int), 1, fd);
	error+= fwrite(&(pattern->description->read_size), sizeof(unsigned long int), 1, fd);
	error+= fwrite(&(pattern->description->write_nb), sizeof(unsigned int), 1, fd);
	error+= fwrite(&(pattern->description->write_size), sizeof(unsigned long int), 1, fd);
	error+= fwrite(&(pattern->description->filenb), sizeof(unsigned int), 1, fd);
	if(error != 7)
		return;
	//the time series
	error = 0;
	for(i = 0; i < pattern->description->reqnb; i++)
	{
		error += fwrite(&(pattern->description->time_series[i].timestamp), sizeof(unsigned long int), 1, fd);
		error += fwrite(&(pattern->description->time_series[i].offset), sizeof(long long int), 1, fd);
	}
	if(error != pattern->description->reqnb*2)
		return;
	//performance information for this access pattern
	//(first we need to know to how many schedulers we have information)
	sched_count=0;
	agios_list_for_each_entry(tmp, &pattern->performance, list)
	{
		sched_count++;
	}
	error = fwrite(&sched_count, sizeof(int), 1, fd);
	if(error != 1)
		return;
	error = 0;
	agios_list_for_each_entry(tmp, &pattern->performance, list)
	{
		error += fwrite(&tmp->sched->index, sizeof(int), 1, fd);
		start = measurements_start;
		measurements=0;
		while(start != measurements_end)
		{
			measurements++;
			start++;
			//TODO continue here
		error += fwrite(
	}
	
	
}
void write_pattern_matching_file(void)
{
	FILE *fd;
	int ret,i;
	struct PM_pattern_t *tmp;

	fd = fopen(config_pattern_filename, "w");
	if(!fd)
	{
		agios_print("Could not open pattern matching file %s\n", config_pattern_filename);
		return;
	}
	else
	{
		//write the number of access patterns
		ret = fwrite(&access_pattern_count, sizeof(unsigned int), 1, fd);		
		if(!ret)
		{
			agios_print("PANIC! Could not write to pattern matching file\n");
			fclose(fd);
			return;	
		}
		agios_list_for_each_entry(tmp, &all_observed_patterns, list)
		{
			write_access_pattern_to_file(tmp, fd);
		}

		//TODO continue here
		fclose(fd);
	}
}
void PATTERN_MATCHING_exit(void)
{
	//write file with what we have learned in this execution
	write_pattern_matching_file();
	//TODO continue here
}
