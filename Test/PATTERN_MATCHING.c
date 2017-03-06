#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "pattern_tracker.h"
//#include "performance.h"
//#include "performance_table.h"
//#include "ARMED_BANDIT.h"
//#include "common_functions.h"
//#include "agios_config.h"
#include "DTW.h"
#include "mylist.h"
#include "PATTERN_MATCHING.h"


#define agios_print(f, a...) 					fprintf(stderr, "AGIOS: " f "\n", ## a)


#ifdef PATTERN_MATCHING_DEBUG
#define PRINT_FUNCTION_NAME agios_print("%s\n", __PRETTY_FUNCTION__)
#define PRINT_FUNCTION_EXIT agios_print("%s exited\n", __PRETTY_FUNCTION__)
#define debug(f, a...) agios_print("%s(): " f "\n", __PRETTY_FUNCTION__, ## a)
#else
#define PRINT_FUNCTION_NAME (void)(0)
#define PRINT_FUNCTION_EXIT (void)(0)
#define debug(f, a...) (void)(0)
#endif


#define config_pattern_filename "/tmp/agios_pattern_matching.dat"

//TODO keep statistics about what happens so we can analyze it
//TODO we write the file at cleanup phase, but in our tests we may not call it. It would be better to have another way of calling it (like when we write stats file)

struct PM_pattern_t *previous_pattern = NULL;
struct PM_pattern_t *predicted_next_pattern=NULL;
AGIOS_LIST_HEAD(all_observed_patterns);
short int first_performance_measurement;
unsigned int access_pattern_count=0;
int current_selection;
unsigned long long int max_dtw_result=1;
unsigned int call_count=0;

int config_minimum_pattern_size=5;
int config_pattern_matching_threshold=80;
int config_maximum_pattern_difference=10;

inline void agios_gettime(struct timespec * timev)
{
	clock_gettime(CLOCK_MONOTONIC, timev);
}
inline unsigned long int get_timespec2llu(struct timespec t)
{
	return (t.tv_sec*1000000000L + t.tv_nsec);
}


//cleanup a PM_pattern_t structure
inline void free_PM_pattern_t(struct PM_pattern_t **pattern)
{
	if((*pattern))
	{
		//free the access pattern description structure
		free_access_pattern_t(&((*pattern)->description));
		//free the performance information structure 
/*		if(!agios_list_empty(&((*pattern)->performance)))
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
		}*/
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
	ret->description->time_series = malloc(sizeof(struct pattern_tracker_req_info_t)*((ret->description->reqnb)+1));
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
/*	init_agios_list_head(&(ret->performance));
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
	}*/
	
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

	//then we read the probability network
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

	//last information: maximum DTW result observed so far (we use it to calculate % difference between patterns)
	ret = fread(&max_dtw_result, sizeof(unsigned long long int), 1, fd);
	if(ret != 1)
	{
		agios_print("Error! Could not read maximum dtw result from pattern matching file\n");
	}	

	//close file
	fclose(fd);
	//free memory
	free(patterns);

	debug("Read %u patterns from the pattern matching file, the maximum DTW score so far is %llu\n", access_pattern_count, max_dtw_result);
}

int PATTERN_MATCHING_init(void)
{
	struct timespec start_time;

	first_performance_measurement = 1;
	//start the pattern tracker
	pattern_tracker_init();

	//read the pattern matching file we keep between executions so we are always smart
	read_pattern_matching_file();

	current_selection = 1;

	PRINT_FUNCTION_EXIT;
	return 1;
}

//apply some heuristics to decide if two access patterns are close enough to compare their time series
inline short int compatible_pattern(struct access_pattern_t *A, struct access_pattern_t *B)
{
	unsigned long diff;

	//let's see the difference in the total number of requests
	if(A->reqnb > 0)
		diff = ((A->reqnb - B->reqnb)*100)/A->reqnb;
	else 
		diff = 100;
	if(diff < 0)
		diff = 0 - diff;
	if(diff > config_maximum_pattern_difference)
		return 0;
	//let`s see the difference in the number of reads
	if(A->read_nb > 0)
		diff = ((A->read_nb - B->read_nb)*100)/A->read_nb;
	else 
		diff = 100;
	if(diff < 0)
		diff = 0 - diff;
	if(diff > config_maximum_pattern_difference)
		return 0;
	//let's see the difference in the number of writes
	if(A->write_nb > 0)
		diff = ((A->write_nb - B->write_nb)*100)/A->write_nb;
	else 
		diff = 100;
	if(diff < 0)
		diff = 0 - diff;
	if(diff > config_maximum_pattern_difference)
		return 0;
	//let's see the difference in the number of accessed files
	if(A->filenb > 0)
		diff = ((A->filenb - B->filenb)*100)/A->filenb;
	else 
		diff = 100;
	if(diff < 0)
		diff = 0 - diff;
	if(diff > config_maximum_pattern_difference)
		return 0;
	//ok, they passed all our tests, so we say they are compatible	
	//TODO more heuristics?
	return 1;
}

//apply FastDTW to get the distance between the two time series (from the two access patterns), and then use the maximum distance ever observed (possibly updating it) to give it a similarity degree between 0 and 100%.
int apply_DTW(struct access_pattern_t *A, struct access_pattern_t *B)
{
	unsigned long long int dtw_result = FastDTW(A, B);
	debug("Compared with DTW to a pattern of %u requests, total amount of accessed data %lu, %u reads (%lu) and %u writes (%lu) with %u files. Got score %llu, which is tranlated to %llu (max is %llu)\n", A->reqnb, A->total_size, A->read_nb, A->read_size, A->write_nb, A->write_size, A->filenb, dtw_result, ((max_dtw_result - dtw_result)*100)/max_dtw_result, max_dtw_result);
	if(dtw_result > max_dtw_result)
		max_dtw_result = dtw_result;
	return ((max_dtw_result - dtw_result)*100)/max_dtw_result;
}

//look for an access pattern in the list of patterns we know
//return NULL if we can't find a match
struct PM_pattern_t *match_seen_pattern(struct access_pattern_t *pattern)
{
	struct PM_pattern_t *tmp, *ret = NULL;
	int best_result = 0.0;
	int this_result;

	PRINT_FUNCTION_NAME;

	agios_list_for_each_entry(tmp, &all_observed_patterns, list)
	{
		//compare the patterns
	 	if(compatible_pattern(tmp->description, pattern)) //first we apply some heuristics so we won't compare all patterns
		{
			this_result = apply_DTW(tmp->description, pattern);
			//among the similar ones, we get the most similar one
			if((this_result >= config_pattern_matching_threshold) && (this_result > best_result)) 
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
struct PM_pattern_t *store_new_access_pattern(struct access_pattern_t *pattern)
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
	if(!found)
	{
		tmp = malloc(sizeof(struct next_patterns_element_t));
		if(!tmp)
		{
			agios_print("PANIC! Could not allocate memory for pattern matching\n");
			return;
		}
		tmp->counter = 0;
		tmp->pattern = new_pattern;
		agios_list_add_tail(&tmp->list, &previous_pattern->next_patterns);
	}
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

int PATTERN_MATCHING_select_next_algorithm(void)
{
	struct access_pattern_t *seen_pattern;
	struct PM_pattern_t *matched_pattern=NULL;
	struct io_scheduler_instance_t *new_sched=NULL;
	struct timespec this_time;
	unsigned long long timestamp;

	PRINT_FUNCTION_NAME;
	
	call_count++;
	
	//get the most recently tracked access pattern from the pattern tracker module
	seen_pattern = get_current_pattern(); 

	debug("Adding a pattern of %u requests, total amount of accessed data %lu, %u reads (%lu) and %u writes (%lu) with %u files\n", seen_pattern->reqnb, seen_pattern->total_size, seen_pattern->read_nb, seen_pattern->read_size, seen_pattern->write_nb, seen_pattern->write_size, seen_pattern->filenb);

	//look for the recently observed pattern in our list of known patterns
	if(seen_pattern->reqnb >= config_minimum_pattern_size) //we ignore patterns which are too small
	{
		matched_pattern = match_seen_pattern(seen_pattern);
		if(matched_pattern) //we don't need the seen_pattern structure anymore, we can free it
		{
			debug("Found a match\n");
			free_access_pattern_t(&seen_pattern);
			//see if our prediction was right
			if(predicted_next_pattern)
			{
				debug("Let's compare this pattern to the one we've predicted. Are they the same? %d. Are they compatible? %d. DTW score: %llu  (because max is %llu)\n", predicted_next_pattern->id == matched_pattern->id, compatible_pattern(predicted_next_pattern->description, matched_pattern->description), FastDTW(predicted_next_pattern->description, matched_pattern->description), max_dtw_result);
			}
		}
		else	//if we did not find the pattern, we'll include it as a new one
		{
			debug("We did not find a match, storing new pattern\n");
			matched_pattern = store_new_access_pattern(seen_pattern);
		}
	}
	else //the pattern is too short, we'll drop it
	{
		debug("We'll ignore this access pattern because it is too short\n");
		free_access_pattern_t(&seen_pattern);
	}

	//we link the previous pattern to this one we've just detected
	if(matched_pattern)
	{
		if(previous_pattern)
			update_probability_network(matched_pattern);
		previous_pattern = matched_pattern;
		//now we can try to predict the next pattern we'll see
		predicted_next_pattern = predict_next_pattern();
	}
	PRINT_FUNCTION_EXIT;
	return current_selection; //if we are running static, we won't ever change the scheduling algorithm, all we do is recognize the pattern
}
//writes information about an access pattern to the file (including performance observed with different scheduling algorithms, NOT including the probability network)
//returns 1 on success, 0 otherwise
int write_access_pattern_to_file(struct PM_pattern_t *pattern, FILE *fd)
{
	int error=0;
	int this_error;
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
		return 0;
	//the time series
	error = 0;
	for(i = 0; i < pattern->description->reqnb; i++)
	{
		error += fwrite(&(pattern->description->time_series[i].timestamp), sizeof(unsigned long int), 1, fd);
		error += fwrite(&(pattern->description->time_series[i].offset), sizeof(long long int), 1, fd);
	}
	if(error != pattern->description->reqnb*2)
		return 0;
	//performance information for this access pattern
	//(first we need to know to how many schedulers we have information)
/*	sched_count=get_sched_info_number(&pattern->performance);
	error = fwrite(&sched_count, sizeof(int), 1, fd);
	if(error != 1)
		return 0;
	error = 0;
	agios_list_for_each_entry(tmp, &pattern->performance, list)
	{
		error += fwrite(&tmp->sched->index, sizeof(int), 1, fd);
		measurements=get_performance_measurements_number(tmp);
		error += fwrite(&measurements, sizeof(int), 1, fd);
		start = tmp->measurements_start;
		this_error = 0;
		while(start != tmp->measurements_end)
		{
			this_error += fwrite(&(tmp->bandwidth_measurements[start].timestamp), sizeof(unsigned long int), 1, fd);
			this_error += fwrite(&(tmp->bandwidth_measurements[start].bandwidth), sizeof(double), 1, fd);
		}	
		if(this_error != measurements*2)
			return 0;
		error += fwrite(&tmp->bandwidth, sizeof(double), 1, fd);
		error += fwrite(&tmp->selection_counter, sizeof(int), 1, fd);
		error += fwrite(&tmp->probability, sizeof(int), 1, fd);
	}
	if(error != 5*sched_count)
		return 0;*/
	return 1;
}
//returns the number of future patters (the size of the provided list)
int get_next_pattern_number(struct agios_list_head *chain)
{
	int ret = 0;
	struct next_patterns_element_t *tmp;
	if(!agios_list_empty(chain))
	{
		agios_list_for_each_entry(tmp, chain, list)
		{
			ret++;
		}
	}
	return ret;
}
void write_pattern_matching_file(void)
{
	FILE *fd;
	int ret, pat_count, error, this_error;
	struct PM_pattern_t *tmp;
	struct next_patterns_element_t *next;

	PRINT_FUNCTION_NAME;

	debug("We'll write the pattern_matching file. We have %u patterns (from %u calls in this execution), max DTW score is %llu\n", access_pattern_count, call_count, max_dtw_result);

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
		//write basic information on the access patterns (general information, the time series and performance observed with scheduling algorithms)
		agios_list_for_each_entry(tmp, &all_observed_patterns, list)
		{
			ret = write_access_pattern_to_file(tmp, fd);
			if(ret != 1)
			{
				agios_print("PANIC! Could not write to pattern matching file!\n");
				fclose(fd);
				break;
			}
		}
		if(ret != 1)
			return;
		//write the probability network, we'll write something about every pattern we know
		error = 0;
		agios_list_for_each_entry(tmp, &all_observed_patterns, list)
		{
			//write the number of future patterns we know for this one
			pat_count = get_next_pattern_number(&tmp->next_patterns);
			error += fwrite(&pat_count, sizeof(int), 1, fd);
			//write the future patterns and associated information
			this_error = 0;
			agios_list_for_each_entry(next, &tmp->next_patterns, list)
			{
				this_error += fwrite(&next->pattern->id, sizeof(int), 1, fd);
				this_error += fwrite(&next->counter, sizeof(unsigned int), 1, fd);
				this_error += fwrite(&next->probability, sizeof(int), 1, fd);
			}	
			if(this_error != pat_count*3)
			{
				agios_print("PANIC! Could not write probability network to pattern matching file\n");
				fclose(fd);
				return;
			}
		}
		if(error != access_pattern_count)
		{
			agios_print("PANIC! Could not write probability network to pattern matching file\n");
			fclose(fd);
			return;
		}

		//last information for the pattern matching file: the maximum dtw difference observed so far
		error = fwrite(&max_dtw_result, sizeof(unsigned long long int), 1, fd);
		if(error != 1)
			agios_print("PANIC! Could not write maximum dtw result to pattern matching file\n");
		fclose(fd);
	}
}
void PATTERN_MATCHING_exit(void)
{
	struct PM_pattern_t *tmp, *aux=NULL;

	//write file with what we have learned in this execution
	write_pattern_matching_file();
	//cleanup data structures pre-allocated for DTW
	DTW_cleanup();
	//cleanup
	agios_list_for_each_entry(tmp, &all_observed_patterns, list)
	{
		if(aux)
		{	
			agios_list_del(&aux->list);
			free_PM_pattern_t(&aux);
		}
		aux = tmp;
	}	
	if(aux)
	{	
		agios_list_del(&aux->list);
		free_PM_pattern_t(&aux);
	}
}
