#ifndef IOSCHED_H
#define IOSCHED_H


#ifdef AGIOS_KERNEL_MODULE
#include <linux/time.h>
#endif
#include "agios.h"
#include "mylist.h"


#define MLF_SCHEDULER 0
#define TIMEORDER_SCHEDULER 1
#define SJF_SCHEDULER 2
#define SRTF_SCHEDULER 3
#define AIOLI_SCHEDULER 4
#define SIMPLE_TIMEORDER_SCHEDULER 5
#define TIME_WINDOW_SCHEDULER 6
#define NOOP_SCHEDULER 7
#define DYN_TREE_SCHEDULER 8
#define ARMED_BANDIT_SCHEDULER 9
#define EXCLUSIVE_TIME_WINDOW 10
#define PATTERN_MATCHING_SCHEDULER 11

struct request_t;
struct request_file_t;

/*
 * Structure describing I/O scheduler operations.
 */
struct io_scheduler_instance_t {

	struct agios_list_head list;
	
	/*
	 * Called when new consumer thread using this scheduler is started.
	 * Intended to allow I/O scheduler to initialized it's private data.
	 */
	int (*init)(void); //must return 1 in success	

	/*
	 * Called when scheduler is no longer use by some thread, but remember
	 * that it can be still used by other threads.
	 */
	void (*exit)(void);
	
	/*
	 * This is the main scheduler function. 
	 */
	void (*schedule)(void *clnt);

	//function provided by dynamic schedulers. It returns the next algorithm to be used
	int (*select_algorithm)(void);

	/*parameters*/
	short int sync; //should we sync after each request (wait until the client finished processing it)
	short int needs_hashtable; //if 0, then timeline is used for requests
	int max_aggreg_size; //maximum number of requests to be aggregated at once
	short int can_be_dynamically_selected; //some algorithms need special conditions (like available trace files or application ids) or are still experimental, so we may not want them to be selected by the dynamic selectors
	short int is_dynamic;
	char name[22];
	int index;

};

//information from the current scheduling algorithm
extern int current_alg;
extern struct io_scheduler_instance_t *current_scheduler;
void change_selected_alg(int new_alg);

//for synchronous scheduling algorithms
void iosched_signal_synchronous(void);
void iosched_wait_synchronous(void);

//so other parts can get statistics
extern long int time_spent_waiting;
extern long int waiting_time_overlapped;


//functions to I/O scheduling algorithm management (setting schedulers, initializing, etc)
int get_algorithm_from_string(const char *alg);
char *get_algorithm_name_from_index(int index);
void register_static_io_schedulers(void);
struct io_scheduler_instance_t *find_io_scheduler(int index);
struct io_scheduler_instance_t *initialize_scheduler(int index);
int get_io_schedulers_size(void);
void enable_TW(void);

//generic functions to be used by multiple scheduling algorithms
void generic_post_process(struct request_t *req);
void generic_cleanup(struct request_t *req);
void generic_init();
void waiting_algorithms_postprocess(struct request_t *req);
void increment_sched_factor(struct request_t *req);
struct request_t *checkSelection(struct request_t *req, struct request_file_t *req_file);
void agios_wait(int  timeout, char *file);
void update_waiting_time_counters(struct request_file_t *req_file, int *smaller_waiting_time, struct request_file_t **swt_file );


/*TODO maybe we could tolerate some useless space between the two aggregated requests, since it is possible that, because of block sizes, this useless portion will be requested anyway*/
#define CHECK_AGGREGATE(req,nextreq) \
     ( (req->io_data.offset <= nextreq->io_data.offset)&& \
         ((req->io_data.offset+req->io_data.len)>=nextreq->io_data.offset))

#endif // #ifndef IOSCHED_H
