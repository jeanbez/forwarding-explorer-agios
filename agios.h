/* File:	agios.h
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the interface to its users
 *		Further information is available at http://inf.ufrgs.br/~fzboito/agios.html
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

#ifndef AGIOS_H
#define AGIOS_H

#ifdef AGIOS_KERNEL_MODULE
#include <linux/completion.h>
#else
#include <pthread.h>
#include <stdint.h>
#endif

#include "iosched.h"		/* for private scheduler data ex. mlf_data */
#include "mylist.h"

//scheduling algorithm options 
#define MAX_MLF_LOCK_TRIES	2
#ifdef ORANGEFS_AGIOS
#define AIOLI_QUANTUM 65536
#else
#define AIOLI_QUANTUM 8192
#endif
#define MLF_QUANTUM 65536   	/*the size of the quantum given to requests when using the qt-mlf scheduling algorithm. 	                 	  *must be set to the expected average size of requests to this server*/
#define TIME_WINDOW_SIZE 1000 // Time window size must be in miliseconds
#define ANTICIPATORY_VALUE(op) (2*get_access_time(AIOLI_QUANTUM,op)) /*the initial quantum given to the requests (usually twice the necessary time to process a request of size MLF_QUANTUM)*/
#define MAX_AGGREG_SIZE   16
#define MAX_AGGREG_SIZE_MLF   200
#define WAIT_AGGREG_CONST 900000   //now in nanoseconds!!!
#define WAIT_SHIFT_CONST 1350000    //now in nanoseconds!!!
#define WAIT_PREDICTED_BETTER 500000 
#define AVERAGE_WAITING_TIME (WAIT_AGGREG_CONST + WAIT_SHIFT_CONST + WAIT_PREDICTED_BETTER)/3.0
#define AGIOS_HASH_SHIFT 6						/* 64 slots in */
#define AGIOS_HASH_ENTRIES		(1 << AGIOS_HASH_SHIFT)		/* hash table  */
#define AGIOS_HASH_FN(inode)	 	(agios_hash_str(inode, AGIOS_HASH_SHIFT))
#define AGIOS_HASH_STR(inode)		(agios_hash_str(inode, AGIOS_HASH_SHIFT))

//if not provided a filename, we'll try to read from this one
#define DEFAULT_CONFIGFILE	"/etc/agios.conf"

/*
 * Request type.
 */
enum {
	RT_READ = 0,
	RT_WRITE = 1,
};

/*
 * Request state.
 */
enum {
	RS_HASHTABLE = 0,
	RS_DISPATCH_QUEUE = 1,
	RS_PROCESSED = 2,
	RS_PREDICTED = 3,
	RS_NONE = 4,
};
/*
 * spatiality of accesses (access pattern aspect)
 */
enum {
	AP_CONTIG = 0,
	AP_NONCONTIG = 1,
};
/*
 * applications' request size (access pattern aspect)
 */
enum {
	AP_SMALL = 0,
	AP_LARGE = 1,
};

struct request_t;

struct client {

	/*
	 * Callback functions
	 */
#ifdef ORANGEFS_AGIOS
	void (*process_request)(int64_t req_id);
	void (*process_requests)(int64_t *reqs, int reqnb);
#else
	void (*process_request)(int i);
	void (*process_requests)(int *i, int reqnb);
#endif

	short int sync; //this parameter is decided by AGIOS, after choosing the scheduling algorithm. 

	/*
	 * aIOLi calls this to check if device holding file is idle.
	 * Should be supported by FS client
	 */
	int (*is_dev_idle)(void);   //never used
};

/*
 * We have one consumer thread running for each block device managed by
 * aIOLi, ex. if you have two hard drive then two consumer threads should
 * be running or if you have data stripping then you need one thread per
 * each device.
 * In general we call all those devices "io device".
 */
struct consumer_t {
#ifdef AGIOS_KERNEL_MODULE
	struct task_struct	*task;
#else
	int task;
#endif


	
	/* IO scheduler used by this consumer. */
	struct io_scheduler_instance_t *io_scheduler;

	struct client		*client;

	int io_device_id;

	struct agios_list_head	list;
};

struct related_list_t {
	struct agios_list_head list ;

	long long used_quantum_rate;

	long long laststartoff ;
									//	we mean 'virtual request'). 
	long long lastfinaloff ;

	long long predictedoff ;
									//	proceed, we compute a predicted offset for the next request
									//	During the next round, if the new request has an offset 
									//	greater than the predict one, a waiting time period is apply.
									//	This approach should fix inefficient shift phenomenon.
									//	Please refer to agios.imag.fr for more informations about 
									//	this problems.
	
	unsigned int lastaggregation ;	// Number of request contained in the last request.
	int proceedreq_nb;		// Number of read or write request proceed since the open 
									// of the file
	
	unsigned long nextquantum; // For QT purpose only
	long long current_size;
	
	struct request_file_t *req_file;

	long double avg_distance;
	int avg_distance_count;
	long double avg_stripe_difference;  //in ms
	short int spatiality;
	short int app_request_size;

	struct {
		unsigned int 	aggs_no;	 /* Number of aggregations proceed on
					  * on this file */
		unsigned int 	sum_of_agg_reqs; /* Sum of aggregated requests */
		unsigned int 	biggest;
		unsigned int 	shift_phenomena;
		unsigned int 	better_aggregation;
		unsigned int 	predicted_better_aggregation;

		unsigned long long int total_req_size;
		unsigned int min_req_size;
		unsigned int max_req_size;
		unsigned long long int max_request_time;
		unsigned long long int total_request_time;
		unsigned long long int min_request_time;
		struct timespec last_req_time;

	} stats;
};

/*
 * This structure is used in hash table to store all request for particular
 * file (inode).
 */
struct request_file_t {
	char *file_id;
	struct related_list_t related_reads;
	struct related_list_t related_writes;
	int timeline_reqnb; //counter for knowing how many requests in the timeline are accessing this file (for scheduling algorithms who do not use hashtable
	struct agios_list_head hashlist;
	struct agios_list_head lru_list; /* To handle stateless filesystems */

	unsigned long long int waiting_time;
	struct timespec waiting_start;


	unsigned long long int first_request_time;
	unsigned long long int first_request_predicted_time;


	struct related_list_t predicted_reads;
	struct related_list_t predicted_writes;

	short int wrote_simplified_trace;
};

typedef void internal_data;

struct io_data {
	long long offset;
	long len;
};
#ifdef ORANGEFS_AGIOS
typedef int64_t user_data_type;
#else
typedef int user_data_type;
#endif

struct request_t { //TODO rethink data types, we probably do not need to have long long long long long long something
	/*parameters*/
	char *file_id;  //name of the file
	unsigned long long int jiffies_64; //arrival time 
	int type; //read or write
	struct io_data io_data; //offset and datasize
	
	int tw_app_id;
	int tw_priority;

	user_data_type data;  /*passed by AGIOS' user (for knowing which request is this one)*/

	/*for the I/O schedulers*/	
	int state;
	unsigned long long int sched_factor; //for MLF and aioli
#ifdef AGIOS_DEBUG
	int sanity;
#endif
	short unsigned int already_waited; /*used by the prediction module when using predicted aggregations to device about waiting times. we do not need to keep it waiting forever, so we just do it once (only the real version - not the predicted - is marked)*/

	/*request's position inside data structures*/
	unsigned int timestamp; //just the arrival order at the scheduler

	struct agios_list_head related; //for including in hashtable or timeline  (see the list implementation)
	
	struct related_list_t *globalinfo; //pointer for the related list inside the file (list of reads, writes, predicted reads or predicted writes)

	//TODO we could separate these specific fields and just put a pointer here, so we would just allocate it when needed
	/*for aggregations*/
	int reqnb; //for virtual requests (real requests), it is the number of requests aggregated into this one. For predicted requests, it is used while reading traces to count how many times this request was predicted (and then reset to 1 after finishing reading)
	struct agios_list_head reqs_list; //list of requests
	struct agios_list_head aggregation_element; //for being inserted on the list of requests
	struct request_t *agg_head;

	/*for predicted requests*/
	struct agios_list_head timeline; //only used for predicted requests (that have both timeline and hashtable). The timeline used by scheduling algorithms use the related field to that
	struct request_t *mirror; //link between real and predicted requests (actually, only the real requests has it)
	struct request_t * predicted_aggregation_start; //link to the predicted request that is the first one of the predicted aggregation
	unsigned int predicted_aggregation_size; //for the first predicted request of a predicted aggregation, how many requests are expected to be aggregated
	unsigned long long int first_agg_time;
	unsigned long long int last_agg_time;

	struct agios_list_head list; 
};

#ifdef AGIOS_KERNEL_MODULE
int __init __agios_init(void);
void __exit __agios_exit(void);
#endif
int agios_init(struct client *clnt, char *config_file);
void agios_exit(void);


#ifdef ORANGEFS_AGIOS
int agios_add_request(char *file_id, int type, long long offset,
		       long len, int64_t data, struct client *clnt);
#else
int agios_add_request(char *file_id, int type, long long offset,
		       long len, int data, struct client *clnt);
#endif


// Try to add the request directly in the dispatch queue
//int dispatch_queue_add(struct request_t *req, struct client *clnt);

void agios_print_stats_file(char *filename);
void agios_reset_stats(void);

void agios_wait_predict_init(void);

#ifndef AGIOS_KERNEL_MODULE
#define ENOMEM 12
#define EINVAL 22
#endif

int get_selected_alg(void);
#endif // #ifndef AGIOS_H
