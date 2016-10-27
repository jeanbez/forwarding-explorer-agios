#include "pattern_tracker.h"
#include "performance.h"
#include "ARMED_BANDIT.h"
#include "PATTERN_MATCHING.h"

struct PM_pattern_t *previous_pattern = NULL;
AGIOS_LIST_HEAD(all_observed_patterns);
short int first_performance_measurement;

//TODO we should discard the first performance measurement

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
	//TODO read file
	init_agios_list_head(&all_observed_patterns);

	return 1;
		
}
int PATTERN_MATCHING_select_next_algorithm(void)
{
	struct PM_pattern_t *seen_pattern;
	int new_sched;
	double *recent_measurements;

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


	//first thing, we need to identify the pattern we've just experienced
	//1 TODO we get something from the pattern tracker module, the representation of the pattern
	//1.1 TODO if the pattern has zero requests, do we just stop? (maybe need to update the previous probabilities first) Maybe we should have a representation for the empty pattern (it could bring some relevant information)?
	//2 TODO we look for this pattern in the all_observed_patterns list, put the match in seen_pattern
	//2.1 TODO we get a performance measurement and save it for the current scheduling algorithm in seen_pattern
	//3 TODO we look for the seen_pattern in the next patterns from previous_pattern so we can update its counters and probability
	//4 TODO based on the probability chain, we predict the next pattern
	//5 TODO based on the next pattern we're predicting, we choose the next scheduling algorithm
	//6 TODO if we can't make predictions, we'll use the Armed Bandit

	//tell ARMED BANDIT which one is the new current scheduler so it will keep updated performance measurements
	ARMED_BANDIT_set_current_sched(new_sched);
	return new_sched;
	
}
void PATTERN_MATCHING_exit(void)
{
	//TODO write file with what we have learned in this execution
}
