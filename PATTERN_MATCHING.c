#include "pattern_tracker.h"
#include "PATTERN_MATCHING.h"

//TODO we should discard the first performance measurement

int PATTERN_MATCHING_init(void)
{
	int ret;

	//start the pattern tracker
	ret = pattern_tracker_init();

	//this is actually a parameter from configuration file, but it won't work as it should if the user did not set it to 0, so lets make sure
	config_agios_select_algorithm_min_reqnumber = 0;
		
}
int PATTERN_MATCHING_select_next_algorithm(void);
void PATTERN_MATCHING_exit(void)
{
	stop_the_pattern_tracker_thread();
}
