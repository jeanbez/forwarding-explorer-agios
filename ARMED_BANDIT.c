/* File:	ARMED_BANDIT.c
 * Created: 	2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 * Collaborators:
 *		Jean Luca Bez <jlbez (at) inf.ufrgs.br>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the Armed Bandit approach 
 *		Further information is available at http://agios.bitbucket.org/
 *
 * Contributors:
 *		Federal University of Rio Grande do Sul (UFRGS)
 *		INRIA France
 *
 *		inspired in Adrien Lebre's aIOLi framework implementation
 *	
 *		This program is distributed in the hope that it will be useful,
 * 		but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "agios_config.h"
#include "iosched.h"
#include "predict.h"
#include "common_functions.h"

static int scheduler_nb=0; //number of possible scheduling algorithms
static int useful_sched_nb=0; //from the possible scheduling algorithms, how many can be dynamically selected (whe have to exclude the dynamic ones, the ones still being tested, etc)
static int benchmarked_scheds=0; //how many of the possible scheduling algorithms were already selected
static int best_bandwidth=-1; //we keep an updated best scheduling algorithm (according to bandwidth) so we don't have to go through all of them when recalculating probabilities

struct scheduler_info_t
{
	struct io_scheduler_instance_t *sched;
	double bandwidth;
	int selection_counter;
	int probability;
};
static struct scheduler_info_t *AB_table=NULL; //all the information we have about the scheduling algorithms
static int current_sched=0; //current scheduling algorithm in use

#define MIN_AB_PROBABILITY 5 //we define a minimum probability to give scheduling algorithms. Without it, we cannot adapt to changes in the workload which could lead to other scheduling algorithm becomes the most adequate. No need to define a maximum probability, since it will be (100 - (useful_sched_nb * MIN_AB_PROBABILITY))

//randomly pick a scheduling algorithm using their probabilities defined in AB table. Returns -1 on error
int randomly_pick_algorithm()
{
	int aggregated_prob=0;
	int i;

	//randomly pick a number between 0 and 99
	int result = rand() % 100;

	//figure to which scheduling algorithm this number corresponds
	//each scheduling algorithm receives a range of values inside [0,100) according to its probability
	//assuming rand() follows an uniform distribution, then result has the right probability of being inside this algorithm's range
	for(i=0; i< scheduler_nb; i++)
	{
		if(AB_table[i].sched->can_be_dynamically_selected == 1)
		{
			aggregated_prob += AB_table[i].probability; 
			if(result < aggregated_prob)
				break;
		}
	}
	if(i == scheduler_nb) //sanity check
	{
		agios_print("PANIC! Could not randomly pick an scheduling algorithm... the random number was %d, aggregated probability is %d", result, aggregated_prob);
		return -1;
	}
	return i;
}
//this function is called during initialization. It initializes data structures used by this algorithm. 
//the first scheduling algorithm is not randomly picked, but defined in AGIOS' configuration file (unless this definition makes no sense)
//returns 1 on success
int ARMED_BANDIT_init(void)
{
	int i;
	struct timespec start_time;

	PRINT_FUNCTION_NAME;

	//initialize the pseudo-random number generator
	agios_gettime(&start_time);
	srand(get_timespec2llu(start_time));

	//find out how many scheduling algorithms do we have
	scheduler_nb = get_io_schedulers_size();
	debug("Initializing Armed Bandit approach with %d scheduling algorithms", scheduler_nb);

	//initialize a table so we can keep information separated by algorithm
	AB_table = malloc(sizeof(struct scheduler_info_t)*(scheduler_nb+1));
	if(!AB_table)
	{
		agios_print("PANIC! Could not allocate memory for the Armed Bandit scheduling table!");
		return -ENOMEM; 
	}
	for(i=0; i< scheduler_nb; i++)
	{
		AB_table[i].sched = find_io_scheduler(i);
		if(AB_table[i].sched->can_be_dynamically_selected == 1)  
			useful_sched_nb++;
		AB_table[i].bandwidth = 0.0;
		AB_table[i].selection_counter=0;
	}

	//give initial probabilities to all scheduling algorithms (which can be selected)
	for(i=0; i < scheduler_nb, i++) //this for loop *cannot* be joined with the previous one, since we use here the useful_sched_nb value (calculated in the previous loop)
	{
		if(AB_table[i].sched->can_be_dynamically_selected == 1)
			AB_table[i].probability = 100 / useful_sched_nb;
	}
	
	//start with the starting algorithm (defined in the configuration file), unless it is something we cannot use here
	current_sched = config_get_starting_algorithm();
	if(AB_table[current_sched].sched->can_be_dynamically_selected != 1)
	{
		agios_print("Error! Starting algorithm with Armed Bandit approach needs to be dynamically selectable.");
		current_sched = randomly_pick_algorithm();
		if(current_sched >= 0)
		{
			agios_print("Changing starting algorithm to %s", AB_table[current_sched].sched->name);
			config_set_starting_algorithm(current_sched);
		}
		else
			return 0;
	}
	AB_table[current_sched].selection_counter++;

	return 1;
}

//since we have performance measurements to all scheduling algorithms, we are able to use these values to give more probability to algorithms which result in better performance
void recalculate_AB_probabilities(void)
{
	int i;
	long double sum_of_bands=0.0;
	int available_probability = 100;

	//calculate a sum of all algorithms' bandwidth
	for(i=0; i< scheduler_nb; i++)
	{
		if(AB_table[i].sched->can_be_dynamically_selected == 1)
			sum_of_bands += AB_table[i].bandwidth;
	}

	//calculate probabilities according to bandwidth
	i = 0;
	while(i < scheduler_nb)
	{
		if(AB_table[i].sched->can_be_dynamically_selected == 1)
		{
			AB_table[i].probability = AB_table[i].bandwidth / sum_of_bands;
			//check if we have met the minimum probability
			if(AB_table[i].probability < MIN_AB_PROBABILITY)
			{
				//TODO
			}	
		}
		i++;
	}
	

	//TODO	
}

//update the observed bandwidth for a scheduling algorithm after using it for a period of time
//we keep a best bandwidth cache (best bandwidth gives the index of the scheduling algorithm for which we have observed the highest bandwidth) to make it easier to recalculate the probabilities
void update_bandwidth(void)
{
	double new_band = get_performance_bandwidth(); 

	if(best_bandwidth == -1) //this is the first algorithm being tested
	{
		best_bandwidth = current_sched;
	}
	else if(best_bandwidth == current_sched) //we are changing the best one's value, so we may need to test all of them again
	{
		if(new_band < AB_table[current_sched].bandwidth) //if we are decreasing the bandwidth, it's possible another algorithm will become the best. When increasing, no need to check
		{
			int i;
			AB_table[current_sched].bandwidth = new_band;
			for(i=0; i< scheduler_nb; i++)
			{
				if(AB_table[i].sched->can_be_dynamically_selected == 1)
				{
					if(AB_table[i].bandwidth > AB_table[best_bandwidth].bandwidth)
						best_bandwidth=i;
				}
			}
		}
	}
	else //we have a best bandwidth and it has not been the current scheduler
	{
		if(new_band > AB_table[best_bandwidth].bandwidth)  //we have observed an even better performance with the current scheduler
			best_bandwidth = current_sched;
	}
	//finally, update the value
	//TODO should/can we use other metrics?
	//TODO should we combine most recently observed value with previous ones, instead of just replacing?
	AB_table[current_sched].bandwidth = new_band;
}

//this function is called periodically to select a new scheduling algorithm
int ARMED_BANDIT_select_next_algorithm(void)
{
	int next_alg=-1;

	PRINT_FUNCTION_NAME;

	//update bandwidth observed for the current scheduling algorithm
	update_bandwidth();

	benchmarked_scheds++;
	//if we haven't tested all algorithms yet, just select one not yet selected
	if(benchmarked_scheds < useful_sched_nb)
	{
		//randomly take one of the useful schedulers (without considering their probabilities for now)
		next_alg = rand() % scheduler_nb;
		while( !((AB_table[next_alg].sched->can_be_dynamically_selected == 1) && (AB_table[next_alg].selection_counter == 0))) 
		{
			next_alg++;
			if(next_alg == scheduler_nb)
				next_alg = 0;
		}
	}
	//we have tested all algorithms at least once, we can use probabilities now
	else
	{
		//recalculate probabilities according to performance values we know
		recalculate_AB_probabilities();

		//select next one following these probabilities
		next_alg = randomly_pick_algorithm();
	}

	//if we were able to select a new algorithm, update its selection counter and our current_sched variable
	if(next_alg >= 0)
	{
		current_sched = next_alg;
		AB_table[current_sched].selection_counter++;	
	}

	return next_alg;
}
int ARMED_BANDIT_exit(void)
{
	if(AB_table)
		free(AB_table);
	return 1;
}
