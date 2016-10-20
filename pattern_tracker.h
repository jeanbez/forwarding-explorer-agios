#ifndef _PATTERN_TRACKER_H_
#define _PATTERN_TRACKER_H_

struct pattern_tracker_req_info_t
{
	unsigned long int timestamp;
	unsigned long int offset;
	unsigned long int len;
	//TODO see with Ramon what we need to track	
	struct agios_list_head list;
};

extern short int agios_is_pattern_tracking;

inline void add_request_to_pattern(unsigned long int timestamp, unsigned long int offset, unsigned long int len);
int pattern_tracker_init();
#endif

