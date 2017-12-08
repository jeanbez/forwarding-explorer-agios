/* File:	agios_request.h
 * Created: 	2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides definitions for the data structures to store information
 * 		about requests, files, etc.
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


#ifndef _AGIOS_REQUEST_H_
#define _AGIOS_REQUEST_H_

#ifdef AGIOS_KERNEL_MODULE
#include <linux/completion.h>
#else
#include <pthread.h>
#include <stdint.h>
#endif

#include "iosched.h"		/* for private scheduler data ex. mlf_data */
#include "mylist.h"


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

struct related_list_statistics_t 
{
	long int processedreq_nb; //number of processed requests 
	long int receivedreq_nb; //number of received requests 

	long int processed_req_size; //total amount of served data
	long int processed_req_time; //time it took to serve the amount of data

	//statistics on request size
	long int total_req_size; //this value is not the same as processed_req_size, since this one is updated when adding a request, and the other is updated in the release function. This means the average request size among received requests is obtained by total_req_size / receivedreq_nb, and the average request size among processed requests is obtained by processed_req_size / processedreq_nb
	long int min_req_size;
	long int max_req_size;

	//statistics on time between requests
	long int total_request_time;
	long int max_request_time;
	long int min_request_time;

	//statistics on average offset difference between consecutive requests
	long double avg_distance;
	long int avg_distance_count;

	//number of performed aggregations and of aggregated requests
	long int 	aggs_no;	 
	long int 	sum_of_agg_reqs; 
};
	

struct related_list_t {
	struct agios_list_head list ; //the queue
	struct agios_list_head dispatch; //the queue of requests which were already scheduled, but not released yet

	struct request_file_t *req_file;

	//for shift phenomenon detection
	long int laststartoff ;
	long int lastfinaloff ;
	long int predictedoff ;

	//for AIOLI
	int nextquantum; 

	//for SJF, SRTF and some statistics
	long int current_size; //sum of all its requests' sizes (even if they overlap)

	//to help decide on waiting times
	int lastaggregation ;	// Number of request contained in the last processed virtual request.
	int 	best_agg; //best aggregation performed to this queue

	struct timespec last_req_time; //so we can keep statistics on time between requests

	long int last_received_finaloffset; //so we can keep statistics on offset distance between consecutive requests 

	//used with dNFSp, do not seem to apply to orangefs
	long double avg_stripe_difference;  //in ms

	//the last detected access pattern for this queue
	short int spatiality;
	short int app_request_size;

	//counters for decisions on waiting times
	long int 	shift_phenomena;
	long int 	better_aggregation;
	long int 	predicted_better_aggregation;

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

	long int timeline_reqnb; //counter for knowing how many requests in the timeline are accessing this file 
	struct agios_list_head hashlist; //to insert this structure in a list (hashtable position or timeline_files)

	//to handle waiting times (they apply to the whole file, not only the queue)
	int waiting_time;
	struct timespec waiting_start;

	//to make arrival times relative
	long int first_request_time;
	long int first_request_predicted_time;

	//used by the prediction module to keep track of generated simplified traces
	short int wrote_simplified_trace;

	int stripe_size; 
};

typedef void internal_data;

struct io_data {
	long int offset;
	long int len;
};
#ifdef ORANGEFS_AGIOS
typedef int64_t user_data_type;
#else
typedef void * user_data_type;
#endif

struct request_t { 
	/*parameters*/
	char *file_id;  //name of the file
	long int jiffies_64; //arrival time 
	long int dispatch_timestamp; //dispatch time 
	short int type; //read or write
	struct io_data io_data; //offset and datasize
	short int state; //used to differentiate between real and predicted requests
	
	//for the TIME WINDOW scheduling algorithm
	int tw_app_id; 
	long int tw_priority; 

	user_data_type data;  /*passed by AGIOS' user (for knowing which request is this one)*/

	//for MLF and aIOLi	
	int sched_factor; 

#ifdef AGIOS_DEBUG
	int sanity;
#endif
	short int already_waited; /*used by the prediction module when using predicted aggregations to device about waiting times. we do not need to keep it waiting forever, so we just do it once (only the real version - not the predicted - is marked)*/

	/*request's position inside data structures*/
	long int timestamp; //just the arrival order at the scheduler

	struct agios_list_head related; //for including in hashtable or timeline  (see the list implementation)
	
	struct related_list_t *globalinfo; //pointer for the related list inside the file (list of reads, writes, predicted reads or predicted writes)

	/*for aggregations*/
	int reqnb; //for virtual requests (real requests), it is the number of requests aggregated into this one. For predicted requests, it is used while reading traces to count how many times this request was predicted (and then reset to 1 after finishing reading)
	struct agios_list_head reqs_list; //list of requests
	struct request_t *agg_head; //pointer to the virtual request structure (if this one is part of an aggregation)

	/*for predicted requests*/
	struct agios_list_head timeline; //only used for predicted requests (that have both timeline and hashtable). The timeline used by scheduling algorithms use the related field to that
	struct request_t *mirror; //link between real and predicted requests (actually, only the real requests has it)
	struct request_t * predicted_aggregation_start; //link to the predicted request that is the first one of the predicted aggregation
	int predicted_aggregation_size; //for the first predicted request of a predicted aggregation, how many requests are expected to be aggregated
	long int first_agg_time;
	long int last_agg_time;

	struct agios_list_head list; 
};

#ifndef AGIOS_KERNEL_MODULE
#define ENOMEM 12
#define EINVAL 22
#endif

#endif
