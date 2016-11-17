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

#include <libconfig.h>
#include <string.h>

#include "agios_config.h"
#include "iosched.h"
#include "common_functions.h"
#include "DYN_TREE.h"
#include "request_cache.h"
#include "proc.h"
#include "consumer.h"
#include "req_hashtable.h"
#include "trace.h"
#include "performance.h"

//-------------------------------------------------------------------------------------------------------
//LIBRARY OPTIONS
//-------------------------------------------------------------------------------------------------------
//since these variables are not static, I try to always include "agios" in their name to avoid future problems
short int config_trace_agios=0;
short int config_trace_agios_predict=0;
short int config_trace_agios_full=0;
short int config_predict_agios_read_traces = 1;
short int config_predict_agios_request_aggregation = 0;
char *config_trace_agios_file_prefix=NULL;
char *config_trace_agios_file_sufix=NULL;
char *config_simple_trace_agios_file_prefix=NULL;
int config_agios_default_algorithm = SJF_SCHEDULER;
int config_predict_agios_time_error = 10;
int config_predict_agios_recalculate_alpha_period = -1;
short int config_agios_write_simplified_traces=0;
char *config_agios_access_times_file=NULL;
long int config_agios_select_algorithm_period=-1;
int config_agios_select_algorithm_min_reqnumber=1;
int config_agios_starting_algorithm = SJF_SCHEDULER;
int config_agios_min_ab_probability = 3;
long int config_agios_validity_window = 360000000000L;
int config_agios_performance_window = 10;
int config_agios_performance_values = 5;
int config_agios_proc_algs = 1000;

int config_waiting_time = 900000;
int config_aioli_quantum = 8192;
int config_mlf_quantum = 8192;
unsigned long int config_tw_size = 1000000000L;
unsigned long int config_exclusive_tw_window_duration=250000000L; //250ms

int config_minimum_pattern_size = 5;
int config_maximum_pattern_difference = 10;
int config_pattern_matching_threshold=75; 
char *config_pattern_filename=NULL;
int config_static_pattern_matching=0;


inline void config_set_waiting_time(int value)
{
	config_waiting_time = value;
}
inline void config_set_aioli_quantum(int value)
{
	config_aioli_quantum = value;
}
inline void config_set_mlf_quantum(int value)
{
	config_mlf_quantum = value;
}
inline void config_set_starting_algorithm(int value)
{
	config_agios_starting_algorithm = value;
	if((config_agios_starting_algorithm == DYN_TREE_SCHEDULER) || (config_agios_starting_algorithm == ARMED_BANDIT_SCHEDULER))
	{
		config_agios_starting_algorithm = SJF_SCHEDULER;
		agios_print("Error! Starting algorithm cannot be a dynamic one. Using SJF instead");
	}
	
}
inline void config_set_trace_predict(short int value)
{
	config_trace_agios_predict = value;
	if((config_trace_agios_predict) && (!config_trace_agios))
		config_trace_agios_predict=0;
}
inline void config_set_trace_full(short int value)
{
	config_trace_agios_full = value;
	if((config_trace_agios_full) && (!config_trace_agios))
		config_trace_agios_full = 0;
}
inline void config_set_predict_request_aggregation(short int value)
{
	config_predict_agios_request_aggregation = value;
	if((config_predict_agios_request_aggregation) && (!config_predict_agios_read_traces))
		config_predict_agios_request_aggregation = 0;
}
inline void config_set_trace_file_name(short int index, const char *value)
{
	if(index == 1)
	{
		config_trace_agios_file_prefix = malloc(sizeof(char)*(strlen(value)+1));
		strcpy(config_trace_agios_file_prefix, value);
	}
	else
	{
		config_trace_agios_file_sufix = malloc(sizeof(char)*(strlen(value)+1));
		strcpy(config_trace_agios_file_sufix, value);
	}
}
inline void config_set_simple_trace_file_prefix(const char *value)
{
	config_simple_trace_agios_file_prefix = malloc(sizeof(char)*(strlen(value)+1));
	strcpy(config_simple_trace_agios_file_prefix, value);
}
inline void config_set_access_times_file(const char *value)
{
	config_agios_access_times_file = (char *)malloc(sizeof(char)*(strlen(value)+1));
	strcpy(config_agios_access_times_file, value);
}
inline void config_set_pattern_filename(const char *value)
{
	config_pattern_filename = (char *)malloc(sizeof(char)*(strlen(value)+1));
	if(!config_pattern_filename)
		agios_print("PANIC! Could not allocate memory for configuration parameters\n");
	else
		strcpy(config_pattern_filename, value);	
}
inline void config_agios_cleanup(void)
{
	if(config_trace_agios_file_prefix)
		free(config_trace_agios_file_prefix);
	if(config_trace_agios_file_sufix)
		free(config_trace_agios_file_sufix);
	if(config_simple_trace_agios_file_prefix)
		free(config_simple_trace_agios_file_prefix);
	if(config_agios_access_times_file)
		free(config_agios_access_times_file);
	if(config_pattern_filename)
		free(config_pattern_filename);
}

//-------------------------------------------------------------------------------------------------------
//USER INFO
//-------------------------------------------------------------------------------------------------------
int config_agios_stripe_size = 32*1024;
unsigned long int config_agios_max_trace_buffer_size = 1*1024*1024;

//-------------------------------------------------------------------------------------------------------
//SPREAD CONFIGURATION PARAMETERS TO OTHER MODULES
//----------------------------------------------------------------------------------------------------------
/*returns 0 in case of success*/
inline short int read_configuration_file(char *config_file)
{
#ifndef AGIOS_KERNEL_MODULE 
	int ret;
	const char *ret_str;
	config_t agios_config;

	config_init(&agios_config);

	/*read configuration file*/
	if((!config_file) || (strlen(config_file) < 1))
		ret = config_read_file(&agios_config, DEFAULT_CONFIGFILE);
	else
		ret = config_read_file(&agios_config, config_file);
	if(ret != CONFIG_TRUE)
	{
		agios_just_print("Error reading agios config file\n%s", config_error_text(&agios_config));
		return 0; //no need to be an error, we'll just run with default values
	}
	/*read configuration*/
	/*1. library options*/
	config_lookup_bool(&agios_config, "library_options.trace", &ret);
	config_trace_agios = ret;
	config_lookup_bool(&agios_config, "library_options.trace_predict", &ret);
	config_set_trace_predict(ret);
	config_lookup_bool(&agios_config, "library_options.trace_full", &ret);
	config_set_trace_full(ret);
	config_lookup_bool(&agios_config, "library_options.predict_read_traces", &ret);
	config_predict_agios_read_traces = ret;
	config_lookup_bool(&agios_config, "library_options.predict_request_aggregation", &ret);
	config_set_predict_request_aggregation(ret);
	config_lookup_string(&agios_config, "library_options.trace_file_prefix", &ret_str);
	config_set_trace_file_name(1,ret_str);
	config_lookup_string(&agios_config, "library_options.trace_file_sufix", &ret_str);
	config_set_trace_file_name(2, ret_str);
	config_lookup_string(&agios_config, "library_options.simple_trace_prefix", &ret_str);
	config_set_simple_trace_file_prefix(ret_str);
	config_lookup_string(&agios_config, "library_options.default_algorithm", &ret_str);
	config_agios_default_algorithm = get_algorithm_from_string(ret_str);
	config_lookup_int(&agios_config, "library_options.prediction_time_error", &config_predict_agios_time_error);
	config_lookup_int(&agios_config, "library_options.prediction_recalculate_alpha_period", &config_predict_agios_recalculate_alpha_period);
	config_lookup_int(&agios_config, "library_options.predict_write_simplified_traces", &ret);
	config_agios_write_simplified_traces = ret;
	config_lookup_string(&agios_config, "library_options.access_times_func_file", &ret_str);
	config_set_access_times_file(ret_str);
	config_lookup_int(&agios_config, "library_options.waiting_time", &ret);
	config_set_waiting_time(ret);
	config_lookup_int(&agios_config, "library_options.aioli_quantum", &ret);
	config_set_aioli_quantum(ret);
	config_lookup_int(&agios_config, "library_options.mlf_quantum", &ret);
	config_set_mlf_quantum(ret);
	config_lookup_int(&agios_config, "library_options.select_algorithm_period", &ret);
	config_agios_select_algorithm_period = ret*1000000L; //convert it to ns
	config_lookup_int(&agios_config, "library_options.select_algorithm_min_reqnumber", &config_agios_select_algorithm_min_reqnumber);
	config_lookup_string(&agios_config, "library_options.starting_algorithm", &ret_str);
	config_set_starting_algorithm(get_algorithm_from_string(ret_str));
	config_lookup_int(&agios_config, "library_options.min_ab_probability", &config_agios_min_ab_probability);
	config_lookup_int(&agios_config, "library_options.validity_window", &ret);
	config_agios_validity_window = ret*1000000L; //convert it to ns
	config_lookup_int(&agios_config, "library_options.performance_window", &config_agios_performance_window);
	config_lookup_int(&agios_config, "library_options.performance_values", &config_agios_performance_values);
	config_lookup_int(&agios_config, "library_options.proc_algs", &config_agios_proc_algs);
	config_lookup_bool(&agios_config, "library_options.enable_TW", &ret);
	if(ret)
		enable_TW();
	config_lookup_int(&agios_config, "library_options.time_window_size", &ret);
	config_tw_size = ret*1000000L; //convert to ns
	config_lookup_int(&agios_config, "library_options.exclusive_tw_window_duration", &ret);
	config_exclusive_tw_window_duration = ret*1000L; //convert us to ns

	config_lookup_int(&agios_config, "library_options.minimum_pattern_size", &ret);
	config_minimum_pattern_size = ret;
	config_lookup_int(&agios_config, "library_options.maximum_pattern_difference", &config_maximum_pattern_difference);
	config_lookup_int(&agios_config, "library_options.pattern_matching_threshold", &config_pattern_matching_threshold);
	config_lookup_string(&agios_config, "library_options.pattern_matching_filename", &ret_str);	
	config_set_pattern_filename(ret_str);
	config_lookup_bool(&agios_config, "library_options.pattern_matching_static_algorithm", &config_static_pattern_matching);

	/*2. user info*/
	config_lookup_int(&agios_config, "user_info.stripe_size", &config_agios_stripe_size);
	config_lookup_int(&agios_config, "user_info.max_trace_buffer_size", &ret);
	config_agios_max_trace_buffer_size = ret*1024; //it comes in KB, we store in bytes
	config_destroy(&agios_config);
#else
	//TODO make these options kernel friendly. For now, we cannot use them in the kernel module version
	config_trace_agios = 0; 
	config_set_trace_predict(0); 
	config_set_trace_full(0); 
	config_set_predict_read_traces(0);

#endif

	config_print();

	return 0;
}

void config_print_yes_or_no(short int flag)
{
	if(flag)
		agios_just_print("YES\n");
	else
		agios_just_print("NO\n");
}
void config_print_flag(short int flag, char *message)
{
	agios_just_print("%s", message);
	config_print_yes_or_no(flag);
}

void config_print(void)
{
	config_print_flag(config_trace_agios, "Will AGIOS generate trace files? ");
	if(config_trace_agios)
	{
		agios_just_print("\tTrace files are named %s.*.%s\n", config_trace_agios_file_prefix, config_trace_agios_file_sufix);
		agios_just_print("\tSimplified trace files are named %s.*.%s\n", config_simple_trace_agios_file_prefix, config_trace_agios_file_sufix);
		config_print_flag(config_trace_agios_predict, "\tTracing the Prediction Module's activities (for debug purposes)? ");
		config_print_flag(config_trace_agios_full, "\tComplete tracing (for debug purposes)? ");
		agios_just_print("\tTrace file buffer has size %lu bytes\n", config_agios_max_trace_buffer_size);
	}
	agios_just_print("Default scheduling algorithm: %s\n", get_algorithm_name_from_index(config_agios_default_algorithm)); 
	if(config_agios_default_algorithm == DYN_TREE_SCHEDULER)
	{	
		agios_just_print("AGIOS will select the best scheduling algorithm for the situation\n");
		if(config_agios_select_algorithm_period >= 0)
			agios_just_print("The scheduling algorithm selection will be redone every %ld nanoseconds, as long as %d requests were processed in this period. We will start with %s\n", 
config_agios_select_algorithm_period, config_agios_select_algorithm_min_reqnumber, get_algorithm_name_from_index(config_agios_starting_algorithm));
		else
			agios_just_print("The scheduling algorithm selection will be done only at the beginning of the execution!\n");
	}
	config_print_flag(config_predict_agios_read_traces, "Will the Prediction Module read information from trace files? ");
	if(config_predict_agios_read_traces)
	{
		config_print_flag(config_predict_agios_request_aggregation, "\tWill the Prediction Module use this information to predict request aggregations? ");
		if(config_predict_agios_request_aggregation)
		{
			agios_just_print("\t\tWill the Prediction Module re-consider its predicted aggregations? ");
			if(config_predict_agios_recalculate_alpha_period == -1)
				agios_just_print("NO\n");
			else
				agios_just_print("Every %d processed requests\n", config_predict_agios_recalculate_alpha_period);
		}
		agios_just_print("\tPredicted requests will be considered the same if their arrival times' difference is within %d\n", config_predict_agios_time_error);
		config_print_flag(config_agios_write_simplified_traces, "\tWill the Prediction Module create simplified traces with the obtained information? ");
	}
	agios_just_print("File with access time functions: %s\n", config_agios_access_times_file);
	agios_just_print("AGIOS thinks its user has stripe size of %d\n", config_agios_stripe_size);	
}


