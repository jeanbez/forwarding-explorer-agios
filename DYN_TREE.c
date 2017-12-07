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
	int next_alg = last_selected_algorithm; //if we can't choose a next one, we keep the same 
	if(config_agios_select_algorithm_period >= 0) //dynamic scheduler
	{
		//TODO
	}
	return next_alg;
}
