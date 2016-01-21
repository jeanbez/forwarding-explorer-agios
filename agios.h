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
enum { //attention, most of these states are deprecated
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
	void (*process_request)(void * i);
	void (*process_requests)(void ** i, int reqnb);
#endif

	/*
	 * aIOLi calls this to check if device holding file is idle.
	 * Should be supported by FS client
	 */
	int (*is_dev_idle)(void);   //never used
};

struct related_list_statistics_t 
{
	unsigned long int processedreq_nb; //number of processed requests 
	unsigned long int receivedreq_nb; //number of received requests 

	unsigned long long int processed_req_size; //total amount of served data
	unsigned long long int processed_req_time; //time it took to serve the amount of data

	//statistics on request size
	unsigned long long int total_req_size; //this value is not the same as processed_req_size, since this one is updated when adding a request, and the other is updated in the release function. This means the average request size among received requests is obtained by total_req_size / receivedreq_nb, and the average request size among processed requests is obtained by processed_req_size / processedreq_nb
	unsigned long int min_req_size;
	unsigned long int max_req_size;

	//statistics on time between requests
	unsigned long long int total_request_time;
	unsigned long int max_request_time;
	unsigned long int min_request_time;

	//statistics on average offset difference between consecutive requests
	long double avg_distance;
	unsigned long int avg_distance_count;

	//number of performed aggregations and of aggregated requests
	unsigned long int 	aggs_no;	 
	unsigned long int 	sum_of_agg_reqs; 
};
	

struct related_list_t {
	struct agios_list_head list ; //the queue
	struct agios_list_head dispatch; //the queue of requests which were already scheduled, but not released yet

	struct request_file_t *req_file;

	//for shift phenomenon detection
	unsigned long int laststartoff ;
	unsigned long int lastfinaloff ;
	unsigned long int predictedoff ;

	//for AIOLI
	unsigned int nextquantum; 

	//for SJF, SRTF and some statistics
	unsigned long int current_size; //sum of all its requests' sizes (even if they overlap)

	//to help decide on waiting times
	unsigned int lastaggregation ;	// Number of request contained in the last processed virtual request.
	unsigned int 	best_agg; //best aggregation performed to this queue

	struct timespec last_req_time; //so we can keep statistics on time between requests

	unsigned long int last_received_finaloffset; //so we can keep statistics on offset distance between consecutive requests 

	//used with dNFSp, do not seem to apply to orangefs
	long double avg_stripe_difference;  //in ms

	//the last detected access pattern for this queue
	short int spatiality;
	short int app_request_size;

	//counters for decisions on waiting times
	unsigned long int 	shift_phenomena;
	unsigned long int 	better_aggregation;
	unsigned long int 	predicted_better_aggregation;

	//statistics to be eventually written in stats file
	struct related_list_statistics_t stats_file;
	
	//statistics to be eventually used to make decisions on scheduling algorithms
	struct related_list_statistics_t stats_window; 
	//we keep both statistics because we will want to reset them at different times. For instance, when we reset statistics used for scheduling algorithm selection, we don't want to lose information that will later go to the stats file
	//predicted lists use stats_file only (because we never reset it)
};

/*
 * This structure is used in hash table to store all request for particular
 * file (inode).
 */
struct request_file_t {
	char *file_id;

	//its queues
	struct related_list_t related_reads;
	struct related_list_t related_writes;
	struct related_list_t predicted_reads;
	struct related_list_t predicted_writes;

	unsigned long int timeline_reqnb; //counter for knowing how many requests in the timeline are accessing this file 
	struct agios_list_head hashlist; //to insert this structure in a list (hashtable position or timeline_files)

	//to handle waiting times (they apply to the whole file, not only the queue)
	unsigned int waiting_time;
	struct timespec waiting_start;

	//to make arrival times relative
	unsigned long int first_request_time;
	unsigned long int first_request_predicted_time;

	//used by the prediction module to keep track of generated simplified traces
	short int wrote_simplified_trace;

	unsigned int stripe_size; 
};

typedef void internal_data;

struct io_data {
	unsigned long int offset;
	unsigned long int len;
};
#ifdef ORANGEFS_AGIOS
typedef int64_t user_data_type;
#else
typedef void * user_data_type;
#endif

struct request_t { 
	/*parameters*/
	char *file_id;  //name of the file
	unsigned long int jiffies_64; //arrival time 
	unsigned long int dispatch_timestamp; //dispatch time 
	short int type; //read or write
	struct io_data io_data; //offset and datasize
	short int state; //used to differentiate between real and predicted requests
	
	//for the TIME WINDOW scheduling algorithm
	unsigned int tw_app_id; 
	unsigned long int tw_priority; 

	user_data_type data;  /*passed by AGIOS' user (for knowing which request is this one)*/

	//for MLF and aIOLi	
	unsigned int sched_factor; 

#ifdef AGIOS_DEBUG
	int sanity;
#endif
	short int already_waited; /*used by the prediction module when using predicted aggregations to device about waiting times. we do not need to keep it waiting forever, so we just do it once (only the real version - not the predicted - is marked)*/

	/*request's position inside data structures*/
	unsigned long int timestamp; //just the arrival order at the scheduler

	struct agios_list_head related; //for including in hashtable or timeline  (see the list implementation)
	
	struct related_list_t *globalinfo; //pointer for the related list inside the file (list of reads, writes, predicted reads or predicted writes)

	/*for aggregations*/
	unsigned int reqnb; //for virtual requests (real requests), it is the number of requests aggregated into this one. For predicted requests, it is used while reading traces to count how many times this request was predicted (and then reset to 1 after finishing reading)
	struct agios_list_head reqs_list; //list of requests
	struct request_t *agg_head; //pointer to the virtual request structure (if this one is part of an aggregation)

	/*for predicted requests*/
	struct agios_list_head timeline; //only used for predicted requests (that have both timeline and hashtable). The timeline used by scheduling algorithms use the related field to that
	struct request_t *mirror; //link between real and predicted requests (actually, only the real requests has it)
	struct request_t * predicted_aggregation_start; //link to the predicted request that is the first one of the predicted aggregation
	unsigned int predicted_aggregation_size; //for the first predicted request of a predicted aggregation, how many requests are expected to be aggregated
	unsigned long int first_agg_time;
	unsigned long int last_agg_time;

	struct agios_list_head list; 
};

#ifdef AGIOS_KERNEL_MODULE
int __init __agios_init(void);
void __exit __agios_exit(void);
#endif
int agios_init(struct client *clnt, char *config_file);
void agios_exit(void);


#ifdef ORANGEFS_AGIOS
int agios_add_request(char *file_id, short int type, unsigned long int offset,
		       unsigned long int len, int64_t data, struct client *clnt);
#else
int agios_add_request(char *file_id, short int type, unsigned long int offset,
		       unsigned long int len, void * data, struct client *clnt);
#endif
int agios_release_request(char *file_id, short int type, unsigned long int len, unsigned long int offset);

int agios_set_stripe_size(char *file_id, unsigned int stripe_size);


void agios_print_stats_file(char *filename);
void agios_reset_stats(void);

void agios_wait_predict_init(void);

#ifndef AGIOS_KERNEL_MODULE
#define ENOMEM 12
#define EINVAL 22
#endif

#endif // #ifndef AGIOS_H
