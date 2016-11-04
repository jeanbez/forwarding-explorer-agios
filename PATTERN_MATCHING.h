#ifndef _PATTERN_MATCHING_H_
#define _PATTERN_MATCHING_H_

#include "mylist.h"

struct PM_pattern_t;

struct next_patterns_element_t {
	struct agios_list_head list;
	unsigned int counter;
	int probability;
	struct PM_pattern_t *pattern;
};

struct PM_pattern_t {
	struct access_pattern_t *description; //information on this access pattern;
	struct agios_list_head list; //to be in a list of all patterns

	struct agios_list_head performance; //a list of scheduler_info_t elements to keep performance measurements to this access pattern with different scheduling algorithms	

	struct agios_list_head next_patterns; //a list of next_patterns_element_t elements, each of them containing a next step and a counter
	unsigned long all_counters; //the sum of all next patterns counts so we can calculate probability
};

int PATTERN_MATCHING_init(void);
int PATTERN_MATCHING_select_next_algorithm(void);
void PATTERN_MATCHING_exit(void);
#endif
