#ifndef _PATTERN_TRACKER_H_
#define _PATTERN_TRACKER_H_

extern short int agios_is_pattern_tracking;

inline void add_request_to_pattern(unsigned long int timestamp, unsigned long int offset, unsigned long int len);
int pattern_tracker_init();
#endif

