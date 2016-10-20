#include "pattern_tracker.h"
#include "PATTERN_MATCHING.h"

struct PM_pattern_t *previous_pattern = NULL;
AGIOS_LIST_HEAD(all_observed_patterns);

//TODO we should discard the first performance measurement

int PATTERN_MATCHING_init(void)
{
	int ret;

	//start the pattern tracker
	ret = pattern_tracker_init();

	//this is actually a parameter from configuration file, but it won't work as it should if the user did not set it to 0, so lets make sure
	config_agios_select_algorithm_min_reqnumber = 0;

	//read the pattern matching file we keep between executions so we are always smart
	//TODO read file
	init_agios_list_head(&all_observed_patterns);
		
}
int PATTERN_MATCHING_select_next_algorithm(void)
{
	struct PM_pattern_t *seen_pattern;

	//first thing, we need to identify the pattern we've just experienced
	//1 TODO we get something from the pattern tracker module, the representation of the pattern
	//1.1 TODO if the pattern has zero requests, do we just stop? (maybe need to update the previous probabilities first) Maybe we should have a representation for the empty pattern (it could bring some relevant information)?
	//2 TODO we look for this pattern in the all_observed_patterns list, put the match in seen_pattern
	//TODO TODO
	
}
void PATTERN_MATCHING_exit(void)
{
	//TODO write file with what we have learned in this execution
}
