#ifndef _PATTERN_TRACKER_H_
#define _PATTERN_TRACKER_H_

#include "mylist.h"

//TODO are we making a static mapping between file handles and starting offsets?


struct pattern_tracker_req_info_t
{
	unsigned long int timestamp;
	long long int offset; //this is actually offset difference from the previous request, so it cannot be unsigned (because it can be negative)
	struct agios_list_head list;
};

struct access_pattern_t {
	//general information about this pattern
	unsigned int reqnb; //number of requests in this pattern
	unsigned long total_size; //total amount of accessed data
	unsigned int read_nb; //number of read requests in this pattern
	unsigned long read_size; //total amount of read data
	unsigned int write_nb; //number of write requests in this pattern
	unsigned long write_size; //total amount of written data
	unsigned int filenb; //how many different files were accessed during this pattern
	
	//request list
	struct agios_list_head requests;
};
	
#define MAXIMUM_FILE_NUMBER 10000 //TODO these numbers were completely arbitrary, make better predictions
#define MAXIMUM_FILE_SIZE 104857600L //in KB	
	

extern short int agios_is_pattern_tracking;

inline void add_request_to_pattern(unsigned long int timestamp, unsigned long int offset, unsigned long int len, short int type, char * file_id);
int pattern_tracker_init();
struct access_pattern_t *get_pattern(void);
#endif

