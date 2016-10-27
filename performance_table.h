#ifndef _PERFORMANCE_TABLE_H_
#define _PERFORMANCE_TABLE_H_

#include "iosched.h"

//struct to hold information of a single performance measurement
struct performance_info_t
{
	unsigned long int timestamp; //the moment this performance information was obtained
	double bandwidth; 
};
//struct to hold multiple performance measurements from a scheduling algorithm
struct scheduler_info_t
{
	struct io_scheduler_instance_t *sched; //the scheduler
	struct performance_info_t *bandwidth_measurements; //the performance measurements in a circle list
	int measurements_start; // start and end pointers to the circle list
	int measurements_end;
	double bandwidth; //average (or median) bandwidth 
	int selection_counter; //how many times was this scheduler selected? used by ARMED_BANDIT
	int probability; //probability associated with this algorithm according to its bandwidth. Used by ARMED_BANDIT
};

inline void reset_scheduler_info(struct scheduler_info_t *info);
inline void add_performance_measurement_to_sched_info(struct scheduler_info_t *info, unsigned long long timestamp, double bandwidth);
short int check_validity_window(struct scheduler_info_t *info, unsigned long int now);
void update_average_bandwidth(struct scheduler_info_t *info);

#endif
