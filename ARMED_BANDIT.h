#ifndef _ARMED_BANDIT_H_
#define _ARMED_BANDIT_H_

int ARMED_BANDIT_init(void);
int ARMED_BANDIT_select_next_algorithm(void);
void ARMED_BANDIT_exit(void);
void write_migration_end(long long int timestamp);
int ARMED_BANDIT_aux_init(struct timespec *start_time);
void ARMED_BANDIT_set_current_sched(int new_sched);
long long int ARMED_BANDIT_update_bandwidth(double *recent_measurements, short int cleanup);
int ARMED_BANDIT_aux_select_next_algorithm(long int timestamp);
#endif
