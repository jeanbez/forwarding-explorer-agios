#include "agios_config.h"
#include "iosched.h"
#include "common_functions.h"
#include "performance.h"

static int scheduler_nb=0; //number of possible scheduling algorithms
static int useful_sched_nb=0; //from the possible scheduling algorithms, how many can be dynamically selected (whe have to exclude the dynamic ones, the ones still being tested, etc)
static int benchmarked_scheds=0; //how many of the possible scheduling algorithms were already selected
static int best_bandwidth=-1; //we keep an updated best scheduling algorithm (according to bandwidth) so we don't have to go through all of them when recalculating probabilities
static int sum_of_all_probabilities=100; //we keep this value so we don't have to recalculate it every time we need it. It may not be 100 because we use integers (for instance, if we start with 6 scheduling algorithms, each one of them will receive probability 100/6 = 16, which sums up to 96)
static long double sum_of_bandwidth=0; //we keep an update sum of all algorithms' bandwidth so we don't have to recalculate it every time we need it. 

struct performance_info_t
{
	unsigned long int timestamp; //the moment this performance information was obtained
	double bandwidth; 
};
struct scheduler_info_t
{
	struct io_scheduler_instance_t *sched;
	struct performance_info_t *bandwidth_measurements;
	int measurements_start;
	int measurements_end;
	double bandwidth;
	int selection_counter;
	int probability;
};
static struct scheduler_info_t *AB_table=NULL; //all the information we have about the scheduling algorithms
static int current_sched=0; //current scheduling algorithm in use

//for debug
void print_all_armed_bandit_information()
{
	int i, j;
	debug("ARMED BANDIT current situation: ");
	for (i=0; i< scheduler_nb; i++)
	{
		if(AB_table[i].sched->can_be_dynamically_selected == 1)
		{
			debug("%s: selected %d times, average bandwidth %.2f, probability %d/%d", AB_table[i].sched->name, AB_table[i].selection_counter, AB_table[i].bandwidth, AB_table[i].probability, sum_of_all_probabilities);
			j = AB_table[i].measurements_start;
			while(j != AB_table[i].measurements_end)
			{
				debug("\t%lu %.2f", AB_table[i].bandwidth_measurements[j].timestamp, AB_table[i].bandwidth_measurements[j].bandwidth); 
				j++;
				if(j == config_agios_performance_window)
					j =0;
			}
		}
		
	}
//	debug("best bandwidth = %s, benchmarked_scheds = %d", AB_table[best_bandwidth].sched->name, benchmarked_scheds);
}

//randomly pick a scheduling algorithm using their probabilities defined in AB table. Returns -1 on error
int randomly_pick_algorithm()
{
	int aggregated_prob=0;
	int i=scheduler_nb;

	//randomly pick a number between 0 and 99 (or less, depending on the sum of all probabilities)
	int result = rand() % sum_of_all_probabilities;

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
		AB_table[i].bandwidth_measurements = (struct performance_info_t *)malloc(sizeof(struct performance_info_t)*config_agios_performance_window);
		AB_table[i].measurements_start = AB_table[i].measurements_end = 0;
	}
	debug("Initializing Armed Bandit approach with %d scheduling algorithms, %d of them can be selected", scheduler_nb, useful_sched_nb );

	//give initial probabilities to all scheduling algorithms (which can be selected)
	sum_of_all_probabilities=0;
	for(i=0; i < scheduler_nb; i++) //this for loop *cannot* be joined with the previous one, since we use here the useful_sched_nb value (calculated in the previous loop)
	{
		if(AB_table[i].sched->can_be_dynamically_selected == 1)
		{
			AB_table[i].probability = 100 / useful_sched_nb;
			sum_of_all_probabilities += AB_table[i].probability;
		}
	}
	
	//start with the starting algorithm (defined in the configuration file), unless it is something we cannot use here
	current_sched = config_agios_starting_algorithm;
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
	int available_probability;

	PRINT_FUNCTION_NAME;
	//adjust the best one's probability proportionally to its bandwidth
	AB_table[best_bandwidth].probability = (AB_table[best_bandwidth].bandwidth*100) / sum_of_bandwidth; //there is no way sum_of_bandwidth is zero because it is updated right before this call

	//make sure this probability is not so large that there is not enough left to give the other algorithms at least MIN_AB_PROBABILITY
	available_probability = 100 - AB_table[best_bandwidth].probability;
	i = (useful_sched_nb - 1)*config_agios_min_ab_probability;
	if(available_probability < i) 
	{
		available_probability = i;
		AB_table[best_bandwidth].probability = 100 - i;
	}
	
	//adjust all other algorithms' probabilities uniformly
	if(useful_sched_nb > 1)
	{
		debug("bandwidth for scheduler %s changed to %.2f, changing probability to %d. Other algorithms will receive probability %d\n", AB_table[best_bandwidth].sched->name, AB_table[best_bandwidth].bandwidth, AB_table[best_bandwidth].probability, available_probability / (useful_sched_nb - 1));
		available_probability = available_probability / (useful_sched_nb - 1);
		for(i=0; i< scheduler_nb; i++)
		{
			if(AB_table[i].sched->can_be_dynamically_selected == 1)
			{
				if(i != best_bandwidth)
					AB_table[i].probability = available_probability;
			}
		}
		sum_of_all_probabilities = AB_table[best_bandwidth].probability + available_probability*(useful_sched_nb - 1); //we divide available_probability by useful_sched_nb - 1 and them multiply again because we are using integers, so this may not sum up to 100.
	}
}

//checks the performance measurement list for a scheduling algorithm, discarding everything older than the validity window 
//receives as parameters the scheduler to be checked and the most recent timestamp;
//returns 1 if something was discarded, 0 if the list if unchanged
short int check_validity_window(int sched, unsigned long int now)
{
	short int changed=0;

	while((AB_table[sched].measurements_start != AB_table[sched].measurements_end) //while we have measurements in the list
	      &&
	      ((now - AB_table[sched].bandwidth_measurements[AB_table[sched].measurements_start].timestamp) >= config_agios_validity_window))
	{
		debug("we will discard a measurement to schedulre %s with timestamp %lu because now we have a timestamp %lu, and the difference %lu is larger than the validity window %lu", AB_table[sched].sched->name, AB_table[sched].bandwidth_measurements[AB_table[sched].measurements_start].timestamp, now, (now - AB_table[sched].bandwidth_measurements[AB_table[sched].measurements_start].timestamp), config_agios_validity_window);
		AB_table[sched].measurements_start++; //discard this measurement, it's too old
		changed=1;
		if(AB_table[sched].measurements_start == config_agios_performance_window)
			AB_table[sched].measurements_start = 0;
	}
	return changed;
}

//updates bandwidth information for a scheduling algorithm by taking the average of its measurements
//this function does not check the validity window, this needs to be done before calling it.
void update_average_bandwidth(int sched)
{
	int i;
	long double ret=0.0;
	int counter=0;

	i = AB_table[sched].measurements_start;
	while(i != AB_table[sched].measurements_end)
	{
		ret += AB_table[sched].bandwidth_measurements[i].bandwidth;
		counter++; 	
		i++;
		if(i == config_agios_performance_window)
			i = 0;
	}
	if(counter > 0) //we could have an empty performance measurement list
		AB_table[sched].bandwidth = ret / counter;
	else
		AB_table[sched].bandwidth = 0.0;
}



//update the observed bandwidth for a scheduling algorithm after using it for a period of time
//we keep a best bandwidth cache (best bandwidth gives the index of the scheduling algorithm for which we have observed the highest bandwidth) to make it easier to recalculate the probabilities
void update_bandwidth(void)
{
	double *recent_measurements;
	int i;
	//int j;
	struct timespec this_time;
	unsigned long long timestamp;

	PRINT_FUNCTION_NAME;

	agios_gettime(&this_time);
	timestamp = get_timespec2llu(this_time);

	// get all recent measurements from the performance model 
	recent_measurements = agios_get_performance_bandwidth();

	// update the recent (other than the current) algorithms' performance measurements (this is necessary because some requests issued by an algorithm may arrive after changing algorithms, resulting in new measurements which were not considered on the last algorithm changes)
/*	i = performance_start;
	while(i != agios_performance_get_latest_index())
	{
		if(recent_measurements[i] > 0)
		{
			if(AB_table[performance_algs[i]].measurements_end != AB_table[performance_algs[i]].measurements_start) //if we have measurements stored for this algorithm (we could have discarded it)
			{
				//get the most recent performance measurement from this algorithm 
				j = AB_table[performance_algs[i]].measurements_end - 1;
				if(j < 0)
					j = PERFORMANCE_WINDOW - 1;
				//compare with the recent performance measurement from the performance module
				if (recent_measurements[i] > AB_table[performance_algs[i]].bandwidth_measurements[j].bandwidth)
				{
					//here we actually have no way of knowing if we are comparing values for the same time this scheduling algorithm executed. We could even rewrite multiple times the same value. We have chosen to make it simpler, that's why we compare before rewritting, so we don't harm algorithms
					AB_table[performance_algs[i]].bandwidth_measurements[j].bandwidth = recent_measurements[i];
					AB_table[performance_algs[i]].bandwidth_measurements[j].timestamp = timestamp;
				}
			}
		}
		i++;
		if(i == PERFORMANCE_VALUES)
			i = 0;
	}*/
	// get new measurement for the current scheduling algorithm
	AB_table[current_sched].bandwidth_measurements[AB_table[current_sched].measurements_end].timestamp = timestamp;
	AB_table[current_sched].bandwidth_measurements[AB_table[current_sched].measurements_end].bandwidth = recent_measurements[agios_performance_get_latest_index()];
	debug("most recent performance measurement: %.2f with scheduler %s", AB_table[current_sched].bandwidth_measurements[AB_table[current_sched].measurements_end].bandwidth, AB_table[current_sched].sched->name);
	AB_table[current_sched].measurements_end++;
	if(AB_table[current_sched].measurements_end == config_agios_performance_window)
	{
		AB_table[current_sched].measurements_end=0;
	}
	if(AB_table[current_sched].measurements_end == AB_table[current_sched].measurements_start)
	{
		AB_table[current_sched].measurements_start++;
		if(AB_table[current_sched].measurements_start == config_agios_performance_window)
			AB_table[current_sched].measurements_start=0;
	}
	check_validity_window(current_sched, timestamp);
	update_average_bandwidth(current_sched); //we need to check validity window and update average bandwidth for the current sched because it is the comparison basis for selecting the best performance algorithm 	

	//go through all scheduling algorithms removing performance measurements which are too old and updating their bandwidths. Also select the best bandwidth and update the sum of all values
	sum_of_bandwidth = 0.0;
	best_bandwidth = current_sched;
	for(i=0; i< scheduler_nb; i++)
	{
		if((AB_table[i].sched->can_be_dynamically_selected == 1) && (AB_table[i].selection_counter > 0))
		{
			if(i != current_sched)
			{
				//discard old performance results and recalculate bandwidth (only if something was discarded)
				if(check_validity_window(i, timestamp))
					update_average_bandwidth(i);
				if(AB_table[i].measurements_start == AB_table[i].measurements_end) //by discarding old results, we may end up without measurements for an algorithm
				{
					debug("we've discarded old results to algorithm %s, so it is untested again", AB_table[i].sched->name);
					benchmarked_scheds--;
					AB_table[i].selection_counter=0;
					continue;
				}
				
				//compare with the best
				if(AB_table[i].bandwidth > AB_table[best_bandwidth].bandwidth)
					best_bandwidth = i;
			}
			//update the sum
			sum_of_bandwidth += AB_table[i].bandwidth;
		}
	}
}

//this function is called periodically to select a new scheduling algorithm
int ARMED_BANDIT_select_next_algorithm(void)
{
	int next_alg=-1;

	PRINT_FUNCTION_NAME;

	print_all_armed_bandit_information();

	//update bandwidth observed for the most recently selected scheduling algorithms
	update_bandwidth();

	print_all_armed_bandit_information();

	debug("we've already tried %d algorithms!", benchmarked_scheds+1);
	//if we haven't tested all algorithms yet, just select one not yet selected
	if(benchmarked_scheds < (useful_sched_nb - 1))
	{
		benchmarked_scheds++;
		//randomly take one of the useful schedulers (without considering their probabilities for now)
		next_alg = rand() % scheduler_nb;
		while( !((AB_table[next_alg].sched->can_be_dynamically_selected == 1) && (AB_table[next_alg].selection_counter == 0))) 
		{
			next_alg++;
			if(next_alg == scheduler_nb)
				next_alg = 0;
		}
		debug("selecting %s because it has not been tried yet", AB_table[next_alg].sched->name);
	}
	//we have tested all algorithms at least once, we can use probabilities now
	else
	{
		//recalculate probabilities according to performance values we know
		recalculate_AB_probabilities();
		print_all_armed_bandit_information();

		//select next one following these probabilities
		next_alg = randomly_pick_algorithm();
		debug("randomly decided to select %s", AB_table[next_alg].sched->name);
	}

	//if we were able to select a new algorithm, update its selection counter and our current_sched variable
	if(next_alg >= 0)
	{
		current_sched = next_alg;
		AB_table[current_sched].selection_counter++;	
	}

	return next_alg;
}
//cleanup function
void ARMED_BANDIT_exit(void)
{
	if(AB_table)
		free(AB_table);
}
