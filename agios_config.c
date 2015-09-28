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

static short int config_trace_agios=0;
static short int config_trace_predict=0;
static short int config_trace_full=0;
static short int config_predict_read_traces = 1;
static short int config_predict_request_aggregation = 0;
static int config_default_algorithm = SJF_SCHEDULER;
static char *config_trace_file_prefix=NULL;
static char *config_trace_file_sufix=NULL;
static char *config_simple_trace_file_prefix=NULL;
static int config_prediction_time_error = 10;
static int config_prediction_recalculate_alpha_period = -1;
static short int config_write_simplified_traces=0;
static char *config_access_times_file=NULL;
static long int config_select_algorithm_period=-1;
static int config_select_algorithm_min_reqnumber=1;
static int config_starting_algorithm;

inline void config_set_select_algorithm_period(int value)
{
	config_select_algorithm_period = value;
	if(config_select_algorithm_period > 0)
		config_select_algorithm_period = config_select_algorithm_period*1000000; //convert it to nanoseconds
	if(config_select_algorithm_period >= 0)
		set_DYN_TREE_dynamic_scheduler(1);
	else
		set_DYN_TREE_dynamic_scheduler(0);
}
inline long int config_get_select_algorithm_period(void)
{
	return config_select_algorithm_period;
}
inline void config_set_select_algorithm_min_reqnumber(int value)
{
	config_select_algorithm_min_reqnumber=value;
}
inline int config_get_select_algorithm_min_reqnumber(void)
{
	return config_select_algorithm_min_reqnumber;
}
inline void config_set_starting_algorithm(int value)
{
	config_starting_algorithm = value;
}
inline int config_get_starting_algorithm(void)
{
	return config_starting_algorithm;
}
inline void config_set_trace(short int value)
{
	config_trace_agios = value;
	//pass this information to other modules
	set_request_cache_trace(config_trace_agios);
	set_iosched_trace(config_trace_agios);
	set_consumer_tracing(config_trace_agios);
}
inline short int config_get_trace()
{
	return config_trace_agios;
}
inline void config_set_trace_predict(short int value)
{
	config_trace_predict = value;
	if((config_trace_predict) && (!config_trace_agios))
		config_trace_predict=0;
	//copy this information to other parts of the code
	set_hashtable_trace_predict(config_trace_predict);
	set_trace_config_trace_predict(config_trace_predict);
}
inline short int config_get_trace_predict(void)
{
	return config_trace_predict;
}
inline void config_set_trace_full(short int value)
{
	config_trace_full = value;
	if((config_trace_full) && (!config_trace_agios))
		config_trace_full = 0;
	//copy this information to other parts
	set_trace_agios_config_trace_full(config_trace_full);
}
inline short int config_get_trace_full(void)
{
	return config_trace_full;
}
inline void config_set_predict_read_traces(short int value)
{
	config_predict_read_traces = value;
}
inline short int config_get_predict_read_traces(void)
{
	return config_predict_read_traces;
}
inline void config_set_predict_request_aggregation(short int value)
{
	config_predict_request_aggregation = value;
	if((config_predict_request_aggregation) && (!config_predict_read_traces))
		config_predict_request_aggregation = 0;
	//pass on this information
	set_request_cache_predict_request_aggregation(config_predict_request_aggregation);
	set_iosched_predict_request_aggregation(config_predict_request_aggregation);
}
inline short int config_get_predict_request_aggregation(void)
{
	return config_predict_request_aggregation;
}
inline void config_set_trace_file_name(short int index, const char *value)
{
	if(index == 1)
	{
		config_trace_file_prefix = malloc(sizeof(char)*(strlen(value)+1));
		strcpy(config_trace_file_prefix, value);
	}
	else
	{
		config_trace_file_sufix = malloc(sizeof(char)*(strlen(value)+1));
		strcpy(config_trace_file_sufix, value);
	}
}
inline char *config_get_trace_file_name(short int index)
{
	if(index == 1)
		return config_trace_file_prefix;
	else
		return config_trace_file_sufix;
}
inline void config_set_simple_trace_file_prefix(const char *value)
{
	config_simple_trace_file_prefix = malloc(sizeof(char)*(strlen(value)+1));
	strcpy(config_simple_trace_file_prefix, value);
}
inline char * config_get_simple_trace_file_prefix(void)
{
	return config_simple_trace_file_prefix;
}
inline void config_set_default_algorithm(int value)
{
	config_default_algorithm = value;
}
inline int config_get_default_algorithm(void)
{
	return config_default_algorithm;
}
inline void config_set_prediction_time_error(int value)
{
	config_prediction_time_error = value;
}
inline int config_get_prediction_time_error(void)
{
	return config_prediction_time_error;
}
inline void config_set_prediction_recalculate_alpha_period(int value)
{
	config_prediction_recalculate_alpha_period = value;
}
inline int config_get_prediction_recalculate_alpha_period()
{
	return config_prediction_recalculate_alpha_period;
}
inline void config_set_write_simplified_traces(short int value)
{
	config_write_simplified_traces = value;
}
inline short int config_get_write_simplified_traces(void)
{
	return config_write_simplified_traces;
}
inline void config_set_access_times_file(const char *value)
{
	config_access_times_file = (char *)malloc(sizeof(char)*(strlen(value)+1));
	strcpy(config_access_times_file, value);
}
inline char *config_get_access_times_file()
{
	return config_access_times_file;
}

//-------------------------------------------------------------------------------------------------------
//USER INFO
//-------------------------------------------------------------------------------------------------------
static int config_stripe_size = 32*1024;
static unsigned long int config_max_trace_buffer_size = 1*1024*1024;

inline void config_set_stripesize(int value)
{
	config_stripe_size = value;
	set_default_stripe_size(value);
}
inline int config_get_stripesize(void)
{
	return config_stripe_size;
}
inline void config_set_max_trace_buffer_size(int value)
{
	config_max_trace_buffer_size = value;
	set_trace_agios_config_max_trace_buffer_size(config_max_trace_buffer_size);
}
inline unsigned long int config_get_max_trace_buffer_size(void)
{
	return config_max_trace_buffer_size;
}

//-------------------------------------------------------------------------------------------------------
//SPREAD CONFIGURATION PARAMETERS TO OTHER MODULES
//-------------------------------------------------------------------------------------------------------
//this function needs to be called when changing the scheduling algorithm. It updates other modules' local copies of configuration paramters
void config_gossip_algorithm_parameters(int alg, struct io_scheduler_instance_t *scheduler)
{
	PRINT_FUNCTION_NAME;
	proc_set_needs_hashtable(scheduler->needs_hashtable);
	set_selected_alg(alg);
	set_max_aggregation_size(scheduler->max_aggreg_size);
	set_needs_hashtable(scheduler->needs_hashtable);
	proc_set_new_algorithm(alg);
	performance_set_needs_hashtable(scheduler->needs_hashtable);
	set_iosched_is_synchronous(scheduler->sync);
}

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
	config_set_trace(ret);
	config_lookup_bool(&agios_config, "library_options.trace_predict", &ret);
	config_set_trace_predict(ret);
	config_lookup_bool(&agios_config, "library_options.trace_full", &ret);
	config_set_trace_full(ret);
	config_lookup_bool(&agios_config, "library_options.predict_read_traces", &ret);
	config_set_predict_read_traces(ret);
	config_lookup_bool(&agios_config, "library_options.predict_request_aggregation", &ret);
	config_set_predict_request_aggregation(ret);
	config_lookup_string(&agios_config, "library_options.trace_file_prefix", &ret_str);
	config_set_trace_file_name(1,ret_str);
	config_lookup_string(&agios_config, "library_options.trace_file_sufix", &ret_str);
	config_set_trace_file_name(2, ret_str);
	config_lookup_string(&agios_config, "library_options.simple_trace_prefix", &ret_str);
	config_set_simple_trace_file_prefix(ret_str);
	config_lookup_string(&agios_config, "library_options.default_algorithm", &ret_str);
	config_set_default_algorithm(get_algorithm_from_string(ret_str));
	config_lookup_int(&agios_config, "library_options.prediction_time_error", &ret);
	config_set_prediction_time_error(ret);
	config_lookup_int(&agios_config, "library_options.prediction_recalculate_alpha_period", &ret);
	config_set_prediction_recalculate_alpha_period(ret);
	config_lookup_int(&agios_config, "library_options.predict_write_simplified_traces", &ret);
	config_set_write_simplified_traces(ret);
	config_lookup_string(&agios_config, "library_options.access_times_func_file", &ret_str);
	config_set_access_times_file(ret_str);
	config_lookup_int(&agios_config, "library_options.select_algorithm_period", &ret);
	config_set_select_algorithm_period(ret); 
	config_lookup_int(&agios_config, "library_options.select_algorithm_min_reqnumber", &ret);
	config_set_select_algorithm_min_reqnumber(ret);
	config_lookup_string(&agios_config, "library_options.starting_algorithm", &ret_str);
	config_set_starting_algorithm(get_algorithm_from_string(ret_str));
	
	
	

	/*2. user info*/
	config_lookup_int(&agios_config, "user_info.stripe_size", &ret);
	config_set_stripesize(ret);
	config_lookup_int(&agios_config, "user_info.max_trace_buffer_size", &ret);
	config_set_max_trace_buffer_size(ret*1024); //it comes in KB, we store in bytes
	
	

	config_destroy(&agios_config);
#else
	//TODO make these options kernel friendly. For now, we cannot use them in the kernel module version
	config_set_trace(0); 
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
		agios_just_print("\tTrace files are named %s.*.%s\n", config_trace_file_prefix, config_trace_file_sufix);
		agios_just_print("\tSimplified trace files are named %s.*.%s\n", config_simple_trace_file_prefix, config_trace_file_sufix);
		config_print_flag(config_trace_predict, "\tTracing the Prediction Module's activities (for debug purposes)? ");
		config_print_flag(config_trace_full, "\tComplete tracing (for debug purposes)? ");
		agios_just_print("\tTrace file buffer has size %lu bytes\n", config_max_trace_buffer_size);
	}
	agios_just_print("Default scheduling algorithm: %s\n", get_algorithm_name_from_index(config_default_algorithm)); 
	if(config_default_algorithm == DYN_TREE_SCHEDULER)
	{	
		agios_just_print("AGIOS will select the best scheduling algorithm for the situation\n");
		if(config_select_algorithm_period >= 0)
			agios_just_print("The scheduling algorithm selection will be redone every %ld nanoseconds, as long as %d requests were processed in this period. We will start with %s\n", 
config_select_algorithm_period, config_select_algorithm_min_reqnumber, get_algorithm_name_from_index(config_starting_algorithm));
		else
			agios_just_print("The scheduling algorithm selection will be done only at the beginning of the execution!\n");
	}
	config_print_flag(config_predict_read_traces, "Will the Prediction Module read information from trace files? ");
	if(config_predict_read_traces)
	{
		config_print_flag(config_predict_request_aggregation, "\tWill the Prediction Module use this information to predict request aggregations? ");
		if(config_predict_request_aggregation)
		{
			agios_just_print("\t\tWill the Prediction Module re-consider its predicted aggregations? ");
			if(config_prediction_recalculate_alpha_period == -1)
				agios_just_print("NO\n");
			else
				agios_just_print("Every %d processed requests\n", config_prediction_recalculate_alpha_period);
		}
		agios_just_print("\tPredicted requests will be considered the same if their arrival times' difference is within %d\n", config_prediction_time_error);
		config_print_flag(config_write_simplified_traces, "\tWill the Prediction Module create simplified traces with the obtained information? ");
	}
	agios_just_print("File with access time functions: %s\n", config_access_times_file);
	agios_just_print("AGIOS' user uses stripe size of %d\n", config_stripe_size);	
}


