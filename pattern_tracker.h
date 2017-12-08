#ifndef _PATTERN_TRACKER_H_
#define _PATTERN_TRACKER_H_

#include "mylist.h"

//TODO are we making a static mapping between file handles and starting offsets?


struct pattern_tracker_req_info_t
{
	long int offset; //this is actually offset difference from the previous request
	struct agios_list_head list;
};

struct access_pattern_t {
	//general information about this pattern
	int reqnb; //number of requests in this pattern
	double total_size; //total amount of accessed data
	int read_nb; //number of read requests in this pattern
	double read_size; //total amount of read data
	int write_nb; //number of write requests in this pattern
	double write_size; //total amount of written data
	int filenb; //how many different files were accessed during this pattern
	
	//request list
	struct agios_list_head requests; //while tracking we use the linked list, because it is easier(and we don't know how many requests we'll receive
	struct pattern_tracker_req_info_t *time_series; //after tracking, we translate it to arrays because it will make DTW easier.

	//used by DTW when creating shrunk version of the time series (which we represent by access pattern structs to facilitate implementation
	int original_size; //request number in the time series from which we shrunk this one
	int *aggPtSize; // vector with the number of requests aggregated in each point 
};
	
#define MAXIMUM_FILE_NUMBER 10000 //TODO these numbers were completely arbitrary, make better predictions
#define MAXIMUM_FILE_SIZE 104857600L //in KB	
	

extern short int agios_is_pattern_tracking;

void add_request_to_pattern(long int timestamp, long int offset, long int len, short int type, char * file_id);
void pattern_tracker_init();
struct access_pattern_t *get_current_pattern(void);
void free_access_pattern_t(struct access_pattern_t **ap);
void reset_current_pattern(short int first_execution);
#endif

