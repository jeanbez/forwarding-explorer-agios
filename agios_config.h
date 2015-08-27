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
inline short int read_configuration_file(char *config_file);

//library options
inline void config_set_trace(short int value);
inline short int config_get_trace(void);
inline void config_set_trace_predict(short int value);
inline short int config_get_trace_predict(void);
inline void config_set_trace_full(short int value);
inline short int config_get_trace_full(void);
inline void config_set_predict_read_traces(short int value);
inline short int config_get_predict_read_traces(void);
inline void config_set_predict_request_aggregation(short int value);
inline short int config_get_predict_request_aggregation(void);
inline void config_set_trace_file_name(short int index, const char *value);
inline char *config_get_trace_file_name(short int index);
inline void config_set_select_algorithm(short int value);
inline short int config_get_select_algorithm(void);
inline void config_set_default_algorithm(int value);
inline int config_get_default_algorithm(void);
inline void config_set_prediction_time_error(int value);
inline int config_get_prediction_time_error(void);
inline void config_set_prediction_recalculate_alpha_period(int value);
inline int config_get_prediction_recalculate_alpha_period(void);
inline void config_set_simple_trace_file_prefix(const char *value);
inline char * config_get_simple_trace_file_prefix(void);
inline void config_set_write_simplified_traces(short int value);
inline short int config_get_write_simplified_traces(void);
inline char *config_get_access_times_file(void);


//user info
inline void config_set_stripesize(int value);
inline int config_get_stripesize(void);
inline void config_set_max_trace_buffer_size(int value);
inline long config_get_max_trace_buffer_size(void);

void config_print(void);
#endif
