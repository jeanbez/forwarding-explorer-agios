#include "agios_config.h"
#include "iosched.h"
#include "common_functions.h"
#include "performance.h"
#include "agios_request.h"

static int scheduler_nb=0; //number of possible scheduling algorithms
static int useful_sched_nb=0; //from the possible scheduling algorithms, how many can be dynamically selected (whe have to exclude the dynamic ones, the ones still being tested, etc)
static int benchmarked_scheds=0; //how many of the possible scheduling algorithms were already selected
static int best_bandwidth=-1; //we keep an updated best scheduling algorithm (according to bandwidth) so we don't have to go through all of them when recalculating probabilities
static int sum_of_all_probabilities=100; //we keep this value so we don't have to recalculate it every time we need it. It may not be 100 because we use integers (for instance, if we start with 6 scheduling algorithms, each one of them will receive probability 100/6 = 16, which sums up to 96)
static long double sum_of_bandwidth=0; //we keep an update sum of all algorithms' bandwidth so we don't have to recalculate it every time we need it. 
static FILE *ab_trace = NULL;

static struct scheduler_info_t *AB_table=NULL; //all the information we have about the scheduling algorithms
static int current_sched=0; //current scheduling algorithm in use
static short int first_performance_measurement;

//This function is called by the PATTERN_MATCHING algorithm because usually the ARMED_BANDIT one keeps its current scheduler updated. However, ARMED_BANDIT is not making the decisions about the current_scheduler if the PATTERN_MATCHING is running, but it needs to know which one is the current one so it will keep update performance measurements.
inline void ARMED_BANDIT_set_current_sched(int new_sched)
{
	current_sched = new_sched;
	AB_table[current_sched].selection_counter++;
}

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
	int i;

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
//for debug, print information to a trace file (only about armed bandit)
void print_ab_trace_probs()
{
	int i, j;
	for(i=0; i < scheduler_nb; i++) 
	{
		if(AB_table[i].sched->can_be_dynamically_selected == 1)
		{
			fprintf(ab_trace, "Scheduler %s\tprob %d/%d\tselected %d times\tavg band %.2f\tMeasurements: ", AB_table[i].sched->name, AB_table[i].probability, sum_of_all_probabilities, AB_table[i].selection_counter, AB_table[i].bandwidth);

			j = AB_table[i].measurements_start;
			while(j != AB_table[i].measurements_end)
			{
				fprintf(ab_trace, "\t%lu %.2f", AB_table[i].bandwidth_measurements[j].timestamp, AB_table[i].bandwidth_measurements[j].bandwidth); 
				j++;
				if(j == config_agios_performance_window)
					j =0;
			}
			fprintf(ab_trace, "\n");

		}
	}
	fprintf(ab_trace, "Selected scheduler %s\n---------------------------------------------------------------------------------------------\n", AB_table[current_sched].sched->name);
	fflush(ab_trace);

}

//this function is different from ARMED_BANDIT_init because it is not called when the ARMED_BANDIT algorithm is being used, but by the PATTERN_MATCHING algorithm which uses ARMED_BANDIT as a helper mechanism. In this situation, when the PATTERN_MATCHING has no information to make a decision, it will use ARMED BANDIT to make it (because it will be still better than random).
int ARMED_BANDIT_aux_init(void)
{
	int i;
	struct timespec start_time;

	PRINT_FUNCTION_NAME;

	first_performance_measurement=1; //we'll need to discard the first measurement because it is not stable yet

	//initialize the pseudo-random number generator
	agios_gettime(&start_time);
	srand(get_timespec2llu(start_time));

	//open ab trace file
	ab_trace = fopen("/tmp/agios_ab_trace.txt", "w");
	if(!ab_trace)
	{
		agios_print("PANIC! Could not open trace file for AB!");
		return -1;
	}

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
		reset_scheduler_info(&AB_table[i]);
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
}

//this function is called during initialization. It initializes data structures used by this algorithm. 
//the first scheduling algorithm is not randomly picked, but defined in AGIOS' configuration file (unless this definition makes no sense)
//returns 1 on success
int ARMED_BANDIT_init(void)
{
	ARMED_BANDIT_aux_init();
	
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
//	AB_table[current_sched].selection_counter++;  (because we'll skip the first window)

	fprintf(ab_trace, "Starting Armed Bandit at timestamp %llu\n", get_timespec2llu(start_time));
	print_ab_trace_probs();

	return 1;
}

//since we have performance measurements to all scheduling algorithms, we are able to use these values to give more probability to algorithms which result in better performance
void recalculate_AB_probabilities(void)
{
	int i, best_i, best_perf;
	int available_probability;
	int *calculated_algs;
	int calculated_counter;

	PRINT_FUNCTION_NAME;

	//we'll use an array to keep track of the algorithms to which we already recalculated the probability
	calculated_algs = malloc(sizeof(int)*scheduler_nb);
	if(!calculated_algs)
	{
		agios_print("PANIC! Could not allocate memory for ARMED BANDIT\n");
		return;
	}
	for(i=0; i< scheduler_nb; i++)
		calculated_algs[i]=0;

	//we'll go through the algorithms in order of performance (best to worst)
	available_probability = 100;
	sum_of_all_probabilities = 0;
	calculated_counter=0;
	while(calculated_counter < useful_sched_nb)
	{
		//get the best performance (among the ones not yet recalculated)
		if(calculated_counter == 0)
			i = best_bandwidth;
		else
		{
			best_perf = 0;
			best_i = 0;
			for(i=0; i < scheduler_nb; i++)
			{
				if((AB_table[i].sched->can_be_dynamically_selected == 1) && (calculated_algs[i] == 0) && (best_perf <= AB_table[i].bandwidth))
				{
					best_perf = AB_table[i].bandwidth;
					best_i = i;
				}
			}
			i = best_i;
		}
		//give this algorithm a probability which is proportional to its bandwidth
		AB_table[i].probability = (AB_table[i].bandwidth*100)/sum_of_bandwidth;
		//give it a bonus according to its position in the ranking
		AB_table[i].probability += (useful_sched_nb - calculated_counter - 1)*3; //3? With 5 algorithms the best will receive +12%, the second +9%, +6%, +3%, and +0%.
		//it cannot be so large that others algorithms won't have the minimum probability (or lower than the minimum)
		if(AB_table[i].probability < config_agios_min_ab_probability)
			AB_table[i].probability = config_agios_min_ab_probability;
		if((available_probability - AB_table[i].probability) < ((useful_sched_nb - calculated_counter - 1)*config_agios_min_ab_probability))
			AB_table[i].probability = available_probability - ((useful_sched_nb - calculated_counter - 1)*config_agios_min_ab_probability);
		//update things
		available_probability -= AB_table[i].probability;
		sum_of_all_probabilities += AB_table[i].probability;
		calculated_algs[i] = 1;
		calculated_counter++;
	}
	free(calculated_algs);
}

//update the observed bandwidth for a scheduling algorithm after using it for a period of time
//we keep a best bandwidth cache (best bandwidth gives the index of the scheduling algorithm for which we have observed the highest bandwidth) to make it easier to recalculate the probabilities
//the cleanup flag says if the function needs to free recent_measurements before returning
unsigned long long int ARMED_BANDIT_update_bandwidth(double *recent_measurements, short int cleanup)
{
	int i;
	//int j;
	struct timespec this_time;
	unsigned long long timestamp;

	PRINT_FUNCTION_NAME;

	agios_gettime(&this_time);
	timestamp = get_timespec2llu(this_time);


	// get new measurement for the current scheduling algorithm
	add_performance_measurement_to_sched_info(&AB_table[current_sched], timestamp, recent_measurements[agios_performance_get_latest_index()]);
	debug("most recent performance measurement: %.2f with scheduler %s", recent_measurements[agios_performance_get_latest_index()], AB_table[current_sched].sched->name);
	//check its performance measurements for measurements which are too old
	if(check_validity_window(&AB_table[current_sched], timestamp))
		debug("we will discard old measurements to scheduler %s\n", AB_table[current_sched].sched->name);
 
	update_average_bandwidth(&AB_table[current_sched]); //we need to check validity window and update average bandwidth for the current sched because it is the comparison basis for selecting the best performance algorithm 	

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
				if(check_validity_window(&AB_table[i], timestamp))
					update_average_bandwidth(&AB_table[i]);
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
	if(cleanup)
		free(recent_measurements)
	return timestamp;
}

//this function does the selection of next algorithm, but without the part where we update performance information. This is useful because the PATTERN_MATCHING algorithm may use ARMED_BANDIT to  make decisions when it does not have enough information to do so itself
int ARMED_BANDIT_aux_select_next_algorithm(unsigned long int timestamp)
{
	int next_alg=-1;
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
		fprintf(ab_trace, "Armed Bandit at timestamp %lu\n", timestamp);
		print_ab_trace_probs();
	}

	return next_alg;
		
}

//this function is called periodically to select a new scheduling algorithm
int ARMED_BANDIT_select_next_algorithm(void)
{
	unsigned long int timestamp;

	PRINT_FUNCTION_NAME;

	print_all_armed_bandit_information();

	//update bandwidth observed for the most recently selected scheduling algorithms
	if(first_performance_measurement == 1) //we won't use the first performance measurement
	{
		struct timespec this_time; //we need to calculate timestamp because we won't call the function ARMED_BANDIT_update_bandwidth, which would normally calculate this for us
		agios_gettime(&this_time);
		timestamp = get_timespec2llu(this_time);
		agios_reset_performance_counters(); //we discard the first measurement
		benchmarked_scheds--;
		first_performance_measurement=0;	
	}
	else
		timestamp = ARMED_BANDIT_update_bandwidth(agios_get_performance_bandwidth(), 1);

	return ARMED_BANDIT_aux_select_next_algorithm(timestamp);

}
void write_migration_end(unsigned long long int timestamp)
{
	fprintf(ab_trace, "Finished migrating data structures at %llu\n-----------------------------------------------------\n", timestamp);
	fflush(ab_trace);
}
//cleanup function
void ARMED_BANDIT_exit(void)
{
	int i;
	if(AB_table)
	{
		for(i = 0; i< scheduler_nb; i++)
		{
			if(AB_table[i].bandwidth_measurements)
				free(AB_table[i].bandwidth_measurements);
		}
		free(AB_table);
	}
	if(ab_trace)
		fclose(ab_trace);
}
