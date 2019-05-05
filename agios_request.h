/*! \file agios_request.h
    \brief Definitions of the data structures used to make queues of requests and to hold information about files (and access statistics).
 */

#pragma once

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

//TODO see how to document a struct

struct request_t;

struct related_list_statistics_t  {
	int64_t processedreq_nb; /**< number of processed requests */
	int64_t receivedreq_nb; /**< number of received requests */
	int64_t processed_req_size; /**< total amount of served data */
	int64_t processed_bandwidth; /**< average bytes per ns */
	int64_t releasedreq_nb; /**< number of released requests */
	//statistics on request size
	int64_t total_req_size; /**< total amount of requested data. This value is NOT the same as processed_req_size, since this one is updated when adding a request, and the other is updated in the release function. This means the average request size among received requests is obtained by total_req_size / receivedreq_nb, and the average request size among processed requests is obtained by processed_req_size / processedreq_nb */
	int64_t min_req_size; /**< largest request size */
	int64_t max_req_size; /**< smallest request size */
	//statistics on time between requests
	int64_t total_request_time; //TODO update arrival rate calculation
	int64_t max_request_time; //TODO
	int64_t min_request_time; //TODO
	long double avg_distance; /**< average offset difference between consecutive requests */
	int64_t avg_distance_count; //TODO
	//number of performed aggregations and of aggregated requests
	int64_t 	aggs_no;	//TODO 
	int64_t 	sum_of_agg_reqs;  //TODO
};
	

struct related_list_t {
	struct agios_list_head list ; /**< the queue of requests */
	struct agios_list_head dispatch; /**< contains requests which were already scheduled, but not released yet */
	struct request_file_t *req_file; /**< a pointer to the struct with information about this file */

	//fields used by aIOLi (and also some of them are used by MLF)
	int64_t laststartoff ; /**< used by aIOLi for shift phenomenon detection */
	int64_t lastfinaloff ; /**< used by aIOLi for shift phenomenon detection */
	int64_t predictedoff ; /**< used by aIOLi for shift phenomenon detection */
	int32_t nextquantum; /**< used by aIOLi to keep track of quanta */
	int64_t 	shift_phenomena; /**< counter used to make decisions regarding waiting times (for aIOLi) */
	int64_t 	better_aggregation; /**< counter used to make decisions regarding waiting times (for aIOLi) */
	int64_t 	predicted_better_aggregation; /**< counter used to make decisions regarding waiting times (for aIOLi) */

	//fields used to keep statistics
	struct related_list_statistics_t stats;  /**< statistics */
	int64_t current_size; /**< sum of all its requests' sizes (even if they overlap). Used by SJF and some statistics */ //TODO
	int32_t lastaggregation ;	/**< Number of request contained in the last processed virtual request. Used to help deciding on waiting times */ //TODO
	int32_t 	best_agg; /**< best aggregation performed to this queue. Used to help deciding on waiting times */ //TODO
	struct timespec last_req_time; /**< timestamp of the last time we received a request for this one, used to keep statistics on time between requests */
	int64_t last_received_finaloffset; /**< offset+len of the last request received to this queue, used to keep statistics on offset distance between consecutive requests */
};

struct request_file_t {
	char *file_id; /**< the file handle */
	struct related_list_t related_reads; /**< read queue */
	struct related_list_t related_writes; /**< write queue */
	int64_t timeline_reqnb; /**< counter for knowing how many requests in the timeline are accessing this file */
	struct agios_list_head hashlist; /**< to insert this structure in a list (hashtable position or timeline_files) */ //TODO
	//used by aIOLi and SJF to handle waiting times (they apply to the whole file, not only the queue)
	int32_t waiting_time; /**< for how long should we be waiting */
	struct timespec waiting_start; /**< since when are we waiting */
	int64_t first_request_time; /**< arrival time of the first request to this file, all requests' arrival times will be relative to this one */
};

struct request_t { 
	char *file_id;  /**< file handle */
	int64_t arrival_time; /**< arrival time of the request to AGIOS */
	int64_t dispatch_timestamp; /**< timestamp of when the request was given back to the user */ 
	int32_t type; /**< RT_READ or RT_WRITE */
	int64_t offset; /**< position of the file in bytes */
	int64_t len; /**< request size in bytes */
	int32_t queue_id; /**< an identifier of the queue to be used for this request, relevant for SW and TWINS only */
	int64_t tw_priority; /**< value calculated by the SW algorithm to insert the request into the queue */
	int64_t user_id;  /**< value passed by AGIOS' user (for knowing which request is this one)*/
	int32_t sched_factor; /**< used by MLF and aIOLi */
	int64_t timestamp; /**< the arrival order at the scheduler (a global value incremented each time a request arrives so the current value is given to that request as its timestamp)*/

	/*request's position inside data structures*/
	struct agios_list_head related; /**< for including in hashtable or timeline */ 
	struct related_list_t *globalinfo; /**< pointer for the related list inside the file (list of reads or  writes) */

	/*for aggregations*/
	int32_t reqnb; /**< for virtual requests (real requests), it is the number of requests aggregated into this one. */
	struct agios_list_head reqs_list; /**< list of requests inside this virtual request*/
	struct request_t *agg_head; /**< pointer to the virtual request structure (if this one is part of an aggregation) */
	struct agios_list_head list;  /**< to be inserted as part of a virtual request */
};


