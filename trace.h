#ifndef AGIOS_KERNEL_MODULE 
#include "agios.h"
#include "agios_request.h"
#include "mylist.h"


#ifndef _TRACE_AGIOS_H_
#define _TRACE_AGIOS_H_


/*will return the number of the current tracefile, or -1 in case of error*/
int agios_trace_init(); 
void agios_trace_reset();
void agios_trace_close();
void agios_trace_cleanup(void);
void agios_trace_add_request(struct request_t *req);

void agios_trace_process_requests(struct request_t *head_req);
void agios_trace_shift(int wait_time, char *file);
void agios_trace_better(char *file);
void agios_trace_predicted_better_aggregation(int wait_time, char *file);
void agios_trace_wait(int wait_time, char *file);

void agios_trace_predict_addreq(struct request_t *req);
void agios_trace_print_predicted_aggregations(struct request_file_t *req_file);

#endif //ifndef _TRACE_AGIOS_H_


#endif //ifndef AGIOS_KERNEL_MODULE
