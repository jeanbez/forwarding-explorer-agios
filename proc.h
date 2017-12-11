#ifndef PROC_H
#define PROC_H

#include "agios_request.h"

void stats_aggregation(struct related_list_t *related);
void stats_shift_phenomenon(struct related_list_t *related);
void stats_better_aggregation(struct related_list_t *related);
void reset_global_reqstats(void);
void stats_predicted_better_aggregation(struct related_list_t *related);
void proc_stats_newreq(struct request_t *req);

void reset_stats_window(void);

int proc_stats_init(void);
void proc_stats_exit(void);

void proc_set_needs_hashtable(short int value);
void proc_set_new_algorithm(int alg);

short int get_window_access_pattern(short int *global_spatiality, short int *global_reqsize, long int *server_reqsize, short int *global_operation);
#endif
