/* File:	trace.h
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		headers for the Trace Module, that generates trace files.
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
#ifndef AGIOS_KERNEL_MODULE 
#include "agios.h"
#include "mylist.h"


#ifndef _TRACE_AGIOS_H_
#define _TRACE_AGIOS_H_


/*will return the number of the current tracefile, or -1 in case of error*/
int agios_trace_init(); 
void agios_trace_reset();
void agios_trace_close();
void agios_trace_add_request(struct request_t *req);

void agios_trace_process_requests(struct request_t *head_req);
void agios_trace_shift(unsigned int wait_time, char *file);
void agios_trace_better(char *file);
void agios_trace_predicted_better_aggregation(unsigned int wait_time, char *file);
void agios_trace_wait(unsigned int wait_time, char *file);

void agios_trace_predict_addreq(struct request_t *req);
void agios_trace_print_predicted_aggregations(struct request_file_t *req_file);

#endif //ifndef _TRACE_AGIOS_H_


#endif //ifndef AGIOS_KERNEL_MODULE
