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
long int *proc_get_alg_timestamps(int *start, int *end);
#endif
