#ifndef _PERFORMANCE_H_
#define _PERFORMANCE_H_

extern long int agios_processed_reqnb;

extern int *performance_algs;
extern int performance_start, performance_end;
void agios_reset_performance_counters(void);
long long int agios_get_performance_size(void);
long long int agios_get_performance_time(void);
double *agios_get_performance_bandwidth();
double agios_get_current_performance_bandwidth(void);
void performance_set_needs_hashtable(short int value);
void performance_set_new_algorithm(int alg);
int agios_performance_get_latest_index();
int agios_performance_init(void);
#endif
