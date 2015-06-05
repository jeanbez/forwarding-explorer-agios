/* File:	predict.h
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It implements the Prediction Module. It is responsible for reading traces,
 *		handling predicted requests, predicting request aggregations, detecting
 *		access pattern's aspects (spatiality and request size), and scheduling
 *		algorithm selection through a decision tree.
 *		Further information is available at http://inf.ufrgs.br/~fzboito/agios.html
 *
 * Contributors:
 *		Federal University of Rio Grande do Sul (UFRGS)
 *		INRIA France
 *	
 *		This program is distributed in the hope that it will be useful,
 * 		but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef _PREDICT_H_
#define _PREDICT_H_

int prediction_module_init(int file_counter);
void predict_init_wait(void);
unsigned long long int get_predict_init_time(void);
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
unsigned long long int agios_predict_should_we_wait(struct request_t *req);
int predict_select_best_algorithm(void);

void calculate_average_stripe_access_time_difference(struct related_list_t *related_list); 
void update_average_distance(struct related_list_t *related_list, long long offset, long len);



#endif
