/* File:	agios_config.c
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It obtains configuration parameters from the configuration files and 
 *		provides them to all other modules
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
#ifndef _AGIOS_CONFIG_H_
#define _AGIOS_CONFIG_H_

#include "iosched.h"

//if not provided a filename, we'll try to read from this one
#define DEFAULT_CONFIGFILE	"/etc/agios.conf"

inline short int read_configuration_file(char *config_file);

//library options
// 1. trace
extern short int config_trace_agios;
extern short int config_trace_agios_predict;
extern short int config_trace_agios_full;
extern char *config_trace_agios_file_prefix;
extern char *config_trace_agios_file_sufix;
extern char *config_simple_trace_agios_file_prefix;
// 2. prediction module
extern short int config_predict_agios_read_traces;
extern short int config_predict_agios_request_aggregation;
extern int config_predict_agios_time_error;
extern int config_predict_agios_recalculate_alpha_period;
extern short int config_agios_write_simplified_traces;
// 3. scheduling
extern int config_agios_default_algorithm;
extern long int config_agios_select_algorithm_period;
extern int config_agios_select_algorithm_min_reqnumber;
extern int config_agios_starting_algorithm;
inline void config_set_starting_algorithm(int value);
// 4. access times estimation
extern char *config_agios_access_times_file;
// 5. ARMED BANDIT
extern int config_agios_min_ab_probability;
extern long int config_agios_validity_window;
extern int config_agios_performance_window;
// 6. performance module
extern int config_agios_performance_values;
// 7. proc module (writing stats file)
extern int config_agios_proc_algs;


//user info
extern int config_agios_stripe_size;
extern unsigned long int config_agios_max_trace_buffer_size;
 

//to spread configuration parameters to all modules
void config_gossip_algorithm_parameters(int alg, struct io_scheduler_instance_t *scheduler);

void config_print(void);

#endif
