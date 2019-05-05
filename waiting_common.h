/*! \file waiting_common.h
    \brief Headers for functions used by aIOLi and MLF.
*/

#pragma once


void update_waiting_time_counters(struct request_file_t *req_file, 
					int *shortest_waiting_time);
bool check_selection(struct request_t *req, 
			struct request_file_t *req_file);
