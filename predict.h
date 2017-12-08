#ifndef _PREDICT_H_
#define _PREDICT_H_

#include "agios_request.h"

int prediction_module_init(int file_counter);
void predict_init_wait(void);
long int get_predict_init_time(void);
void stop_prediction_thr(void);
void set_predict_request_aggregation_flag(int value);
void lock_prediction_thr_refresh_mutex(void);
void unlock_prediction_thr_refresh_mutex(void);
void refresh_predictions(void);
void prediction_notify_new_trace_file(void);
int get_predict_tracefile_counter(void);


void inc_current_predicted_reqfilenb(void);
int get_current_predicted_reqfilenb(void);

void prediction_newreq(struct request_t *req);
long int agios_predict_should_we_wait(struct request_t *req);
int predict_select_best_algorithm(void);

void calculate_average_stripe_access_time_difference(struct related_list_t *related_list); 
void update_average_distance(struct related_list_t *related_list, long offset, long len);



#endif
