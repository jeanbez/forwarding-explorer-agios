/* File:	DYN_TREE.c
 * Created: 	2014 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the dynamic scheduling algorithm selection through selection trees 
 *		(the algorithm used in Francieli's thesis)
 *
 * Contributors:
 *		Federal University of Rio Grande do Sul (UFRGS)
 *		INRIA France
 *
 *		This program is distributed in the hope that it will be useful,
 * 		but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "agios_config.h"
#include "iosched.h"
#include "predict.h"
#include "common_functions.h"
#include "agios_request.h"

static int last_selected_algorithm=0;

//returns 0 on success
int DYN_TREE_init(void)
{
	int ret;
	//we are initializing the DYN_TREE scheduling algorithm. If we plan to keep using it to select scheduling algorithms during the execution, we do nothing at this point since we need to wait until we have enough data to make a decision. On the other hand, if we are just selecting the algorithm at the beginning of the execution, we make the decision based on traces
	if(config_agios_select_algorithm_period < 0) //NOT dynamic scheduler 	
	{
		//first we need to wait for the prediction module to finish making its predictions
		agios_wait_predict_init();
		//ask the prediction module to make this decision based on its information
		ret = predict_select_best_algorithm();
		if(ret >= 0) //it could return -1 if it is not able to decide*/
		{
			debug("DYN_TREE selected the %s algorithm", get_algorithm_name_from_index(ret));
			config_set_starting_algorithm(ret);
			last_selected_algorithm = ret;
		}
		else
		{
			last_selected_algorithm = config_agios_starting_algorithm;
			agios_print("DYN_TREE Could not make a decision about scheduling algorithm, using starting one %s\n", get_algorithm_name_from_index(config_agios_starting_algorithm));	
		}
	}
	else
		last_selected_algorithm = config_agios_starting_algorithm;
	return 0;	
}
int DYN_TREE_select_next_algorithm(void)
{
	int operation;
	long int reqsize;
	short int reqsize_class;
	double timediff;
	short int spatiality;
	double avgdist;
	

	if(config_agios_select_algorithm_period >= 0) //dynamic scheduler
	{
		//get information from the current window 
		operation = get_global_window_operation();
		reqsize = get_global_reqsize();
		
		if((operation < 0) || (reqsize <= 0)) //if we dont have information, we don't make a decision
		{
			agios_print("Warning! DYN THREE does not have enough information to make a decision");
		}
		else
		{
			//we have to classify reqsize into small or large. In the prediction module this is done using the avg stripe difference, but that does not make sense in the context of systems like OrangeFS. Instead, we'll use the stripe size as a threshold, and then give to the access pattern detection tree a value that would make it conclude on this class.
			if(reqsize <= config_agios_stripe_size)
			{
				timediff = 1000.0; //small request
				reqsize_class = AP_SMALL;
			}
			else
			{
				timediff = 0.0; //large request
				reqsize_class = AP_LARGE;
			}
			//get spatiality
			get_global_spatiality(operation,timediff); //TODO continue here
			
			//use the decision tree to select a new algorithm
			last_selected_algorithm = get_algorithm_from_string(scheduling_algorithm_selection_tree(operation, current_reqfilenb, get_access_ratio(reqsize, operation), spatiality, reqsize_class)); 
		}
	}
	return last_selected_algorithm;
}
