/* File:	proc.c
 * Created: 	2012
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It handles scheduling statistics and print them whenever the agios_print_stats_file
 *		function is called. Stats can be reseted by agios_reset_stats.
 *		The kernel module implementation uses the proc interface, while the user-level
 *		library creates a file in the local file system. Its name is provided as a parameter.
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
#include "agios.h"
#include "agios_request.h"
#include "proc.h"
#include "mylist.h"
#include "common_functions.h"
#include "trace.h"
#include "request_cache.h"
#include "predict.h"
#include "agios_config.h"
#include "req_hashtable.h"
#include "req_timeline.h"
#include "performance.h"
#include "hash.h"

#ifdef AGIOS_KERNEL_MODULE
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#else
#include <string.h>
#include <limits.h>
#endif

/***********************************************************************************************************
 * GLOBAL ACCESS PATTERN STATISTICS *
 ***********************************************************************************************************/
struct global_statistics_t
{
	long int total_reqnb; //we have a similar counter in consumer.c, but this one can be reset, that one is fixed (never set to 0, counts through the whole execution)
//to measure time between requests
	long int reads;
	long int writes;
	long int global_req_time;
	long int global_min_req_time;
	long int global_max_req_time;
	//to measure request size
	long int global_req_size;
	long int global_min_req_size;
	long int global_max_req_size;
};
static struct timespec last_req;
static struct global_statistics_t stats_for_file;
static struct global_statistics_t stats_for_window;
static pthread_mutex_t global_statistics_mutex = PTHREAD_MUTEX_INITIALIZER;



/***********************************************************************************************************
 * LOCAL COPIES OF PARAMETERS *
 ***********************************************************************************************************/
//this module keeps a list of all selected algorithms and the timestamp of selection (mostly used for debugging the dynamic schedulers)
struct proc_alg_entry_t
{
	long int timestamp;
	int alg;
	struct agios_list_head list;
};
static AGIOS_LIST_HEAD(proc_algs);
static int proc_algs_len=0;

void proc_set_new_algorithm(int alg)
{
	struct timespec now;
	struct proc_alg_entry_t *new = malloc(sizeof(struct proc_alg_entry_t));

	agios_gettime(&now);
	new->timestamp = get_timespec2long(now);
	new->alg = alg;
	agios_list_add_tail(&new->list, &proc_algs);
	proc_algs_len++;

	//need to keep the configured maximum size for the list
	while(proc_algs_len > config_agios_proc_algs)
	{
		agios_list_for_each_entry(new, &proc_algs, list)
			break; //we get the first element of the list
		agios_list_del(&new->list);
		free(new);
	}
}
long int *proc_get_alg_timestamps(int *start, int *end)
{
	*start = proc_algs_start;
	*end = proc_algs_end;
	return proc_algs_timestamps;
}



/***********************************************************************************************************
 * STATISTICS FILE *
 ***********************************************************************************************************/
/*the user-level library uses a file, while the kernel module implementation uses the proc file system interface*/
#ifndef AGIOS_KERNEL_MODULE
static FILE *stats_file;
#else
static struct seq_file *stats_file; //to help while writing the file
static struct proc_dir_entry *agios_proc_dir;
static struct file_operations stats_proc_ops;
static struct file_operations predicted_stats_proc_ops;
#ifdef AGIOS_DEBUG
static struct file_operations hashtable_proc_ops;
#endif
#endif
static int hash_position;


/***********************************************************************************************************
 * FUNCTIONS TO UPDATE STATISTICS UPON EVENTS *
 ***********************************************************************************************************/
void update_local_stats(struct related_list_statistics_t *stats, struct request_t *req)
{
	long int elapsedtime=0;
	long int this_distance;

	//update local statistics on time between requests
	if(stats->total_request_time == 0)
		stats->total_request_time = 1;
	else
	{
		elapsedtime = (req->jiffies_64 - get_timespec2long(req->globalinfo->last_req_time))/1000; //nano to microseconds
		if(stats->total_request_time == 1)
			stats->total_request_time = 0;

		stats->total_request_time += elapsedtime;

		if(elapsedtime > stats->max_request_time)
			stats->max_request_time = elapsedtime;
		if(elapsedtime < stats->min_request_time)
			stats->min_request_time = elapsedtime;
	}
	get_long2timespec(req->jiffies_64, &req->globalinfo->last_req_time);

	//update local statistics on average offset distance between consecutive requests
	if(req->globalinfo->last_received_finaloffset > 0)
	{
		if(stats->avg_distance == -1)
			stats->avg_distance = 0;
		this_distance = req->io_data.offset - req->globalinfo->last_received_finaloffset;
		if(this_distance < 0)
			this_distance *= -1;
		this_distance = this_distance/ req->io_data.len; //should we use the last request's size instead of this one? or maybe the average request size of this queue?
		stats->avg_distance += this_distance;
		stats->avg_distance_count++;
	}

	//update local statistics on request size
	stats->total_req_size+=req->io_data.len;
	if(req->io_data.len > stats->max_req_size)
		stats->max_req_size = req->io_data.len;
	if(req->io_data.len < stats->min_req_size)
		stats->min_req_size = req->io_data.len;
}
void update_global_stats_newreq(struct global_statistics_t *stats, struct request_t *req, long int elapsedtime)
{

	stats->total_reqnb++;
	//update global statistics on time between requests
	if((stats->total_reqnb > 1) && (elapsedtime > 0))
	{
		stats->global_req_time += elapsedtime;
		if(elapsedtime > stats->global_max_req_time)
			stats->global_max_req_time = elapsedtime;
		if(elapsedtime < stats->global_min_req_time)
			stats->global_min_req_time = elapsedtime;
	}
	//update global statistics on request size
	stats->global_req_size += req->io_data.len;
	if(req->io_data.len > stats->global_max_req_size)
		stats->global_max_req_size = req->io_data.len;
	if(req->io_data.len < stats->global_min_req_size)
		stats->global_min_req_size = req->io_data.len;
	//update global statistics on operation
	if(req->type == RT_READ)
		stats->reads++;
	else
		stats->writes++;
}
/*update the stats after the arrival of a new request
 * must hold the hashtable mutex
 */
void proc_stats_newreq(struct request_t *req)
{
	long int elapsedtime=0;

	req->globalinfo->stats_file.receivedreq_nb++;
	req->globalinfo->stats_window.receivedreq_nb++;

	if(req->state == RS_PREDICTED)
	{
		req->globalinfo->stats_file.processedreq_nb++;
		req->globalinfo->stats_window.processedreq_nb++;
	}
	else //if req->state is RS_HASHTABLE
	{
		agios_mutex_lock(&global_statistics_mutex);

		//update global statistics
		if(req->jiffies_64 == req->globalinfo->req_file->first_request_time)
			elapsedtime=0;
		else
			elapsedtime = (req->jiffies_64 - get_timespec2long(last_req)) / 1000; //nano to microseconds
		update_global_stats_newreq(&stats_for_file, req, elapsedtime);
		update_global_stats_newreq(&stats_for_window, req, elapsedtime);
		get_long2timespec(req->jiffies_64, &last_req);

		agios_mutex_unlock(&global_statistics_mutex);

		//update local statistics
		update_local_stats(&req->globalinfo->stats_file, req);
		update_local_stats(&req->globalinfo->stats_window, req);


		req->globalinfo->last_received_finaloffset = req->io_data.offset + req->io_data.len;
	}
}

/*reset some global stats*/
void _reset_global_reqstats(struct global_statistics_t *stats)
{
	stats->total_reqnb=0;
	stats->global_req_time = 0;
	stats->global_min_req_time = LONG_MAX;
	stats->global_max_req_time = 0;
	stats->global_req_size = 0;
	stats->global_min_req_size = LONG_MAX;
	stats->global_max_req_size=0;
	stats->reads = 0;
	stats->writes = 0;
}
void reset_global_reqstats(void)
{
	agios_mutex_lock(&global_statistics_mutex);
	_reset_global_reqstats(&stats_for_file);
	_reset_global_reqstats(&stats_for_window);
	agios_mutex_unlock(&global_statistics_mutex);
}
void reset_global_reqstats_window(void)
{
	agios_mutex_lock(&global_statistics_mutex);
	_reset_global_reqstats(&stats_for_window);
	agios_mutex_unlock(&global_statistics_mutex);
}
void reset_global_reqstats_file(void)
{
	agios_mutex_lock(&global_statistics_mutex);
	_reset_global_reqstats(&stats_for_file);
	agios_mutex_unlock(&global_statistics_mutex);
}
/*reset statistics used for scheduling algorithm selection*/
//this is called while holding the migration mutex algorithm, so no other mutexes are necessary
void reset_stats_window_related_list(struct related_list_t *related_list)
{
	related_list->stats_window.processedreq_nb = 0;
	related_list->stats_window.receivedreq_nb = 0;
	related_list->stats_window.processed_req_size = 0;
	related_list->stats_window.processed_req_time = 0;

	related_list->stats_window.total_req_size=0;
	related_list->stats_window.min_req_size=LONG_MAX;
	related_list->stats_window.max_req_size=0;

	related_list->stats_window.max_request_time = 0;
	related_list->stats_window.total_request_time = 0;
	related_list->stats_window.min_request_time = LONG_MAX;

	related_list->stats_window.avg_distance=0;
	related_list->stats_window.avg_distance_count=1;

	related_list->stats_window.aggs_no = 0;
	related_list->stats_window.sum_of_agg_reqs = 0;
}
void reset_stats_window(void)
{
	int i;
	struct agios_list_head *list;
	struct request_file_t *req_file;

	for(i=0; i< AGIOS_HASH_ENTRIES; i++)
	{
		list = &hashlist[i];
		agios_list_for_each_entry(req_file, list, hashlist)
		{
			reset_stats_window_related_list(&req_file->related_reads);
			reset_stats_window_related_list(&req_file->related_writes);
		}
	}
	reset_global_reqstats_window();
}

/*updates the stats after an aggregation*/
void stats_aggregation(struct related_list_t *related)
{
	if (related->lastaggregation > 1) {
		related->stats_file.aggs_no++;
		related->stats_file.sum_of_agg_reqs += related->lastaggregation;
		related->stats_window.aggs_no++;
		related->stats_window.sum_of_agg_reqs += related->lastaggregation;
		if (related->best_agg < related->lastaggregation)
			related->best_agg = related->lastaggregation;
	}
}
/*updates the stats after the detection of a shift phenomenon*/
void stats_shift_phenomenon(struct related_list_t *related)
{
	related->shift_phenomena++;
}
/*updates the stats after the detection that a better aggregation is possible (by looking to the current stats for the file)*/
void stats_better_aggregation(struct related_list_t *related)
{
	related->better_aggregation++;
}
/*updates the stats after the detection that a better aggregation is possible (by looking at the predictions)*/
void stats_predicted_better_aggregation(struct related_list_t *related)
{
	related->predicted_better_aggregation++;
}

/***********************************************************************************************************
 * FUNCTIONS TO WRITE THE STATS FILE *
 ***********************************************************************************************************/
//the functions that write the stats file are organized in a way as to be used by both library and kernel module implementations. The kernel module implementation uses the proc interface. The functions iterate through statistics list.
//get_first points the first element (and locks the relevant mutex)
struct request_file_t *get_first(short int predicted_stats)
{
	struct agios_list_head *reqfile_l;
	struct request_file_t *req_file=NULL;

	while(!req_file)
	{
		if((current_scheduler->needs_hashtable) || (predicted_stats))
			hashtable_lock(hash_position);
		reqfile_l = &hashlist[hash_position];

		if(!agios_list_empty(reqfile_l))
		{ //take the first element of the list
			agios_list_for_each_entry(req_file, reqfile_l, hashlist)
				break;
		}
		else
		{//list is empty, nothing to show, unlock the mutex
			if((current_scheduler->needs_hashtable) || (predicted_stats))
			{
				hashtable_unlock(hash_position);
			}
			hash_position++;
			if(hash_position >= AGIOS_HASH_ENTRIES)
			{
				timeline_unlock();
				break;
			}
		}
	}
	return req_file;
}

//we use stats_window (the one that is eventually reseted) to detect access pattern 
//returns -1 if receivedreq_nb is 0 (this is not supposed to happen, but...)
short int get_list_access_pattern(struct related_list_t *related, short int operation, short int *spatiality, short int *reqsize)
{
	double avgdiff;
	double timediff;
	long int avgreqsize;

	//calculate average offset distance
	if(related->stats_window.avg_distance_count > 1)
		avgdiff = ((long double) related->stats_window.avg_distance)/(related->stats_window.avg_distance_count-1);
	else
		avgdiff = 0;

	//calculate request size
	if(related->stats_window.receivedreq_nb <= 0)
		return -1;
	avgreqsize = related->stats_window.total_req_size/related->stats_window.receivedreq_nb;

	//the prediction module uses the stripe time difference to detect request size, but that does not make sense in the context of the OrangeFS, so we will use the stripe size as a threshold and come up with a value for stripe time difference that gives the right request size
	if(reqsize <= config_agios_stripe_size)
		timediff = 1000.0; //small request
	else
		timediff = 0.0; //large request
	
	access_pattern_detection_tree(operation, avgdiff, timediff, spatiality, reqsize);	
	return 0;
}

//we need to acquire the locks for the data structures because they could be changed during the execution of this function (other threads could be adding requests and thus adding new request_file_t entries, that could lead to us getting lost here). But we don't need to concern ourselves with the data structure changing because this function is called by the scheduling thread before changing scheduling algorithms. ATTENTION: if this function is called by other thread, need to think about that!
//returns -1 if we dont have enough information to decide
short int get_window_access_pattern(short int *global_spatiality, short int *global_reqsize, long int *server_reqsize, short int *global_operation)
{
	int i;
	struct request_file_t *req_file;
	struct related_list_t *related;
	short int reqsize;
	short int spatiality;
	//well count different outcomes
	int read_spatiality_count[2] = {0,0};
	int write_spatiality_count[2] = {0,0};
	int read_reqsize_count[2] = {0,0};
	int write_reqsize_count[2] = {0,0};


	if((stats_for_window->writes == stats_for_window->reads == 0) || (stats_for_window->total_reqnb == 0))
		return -1; //we do not have information

	*server_reqsize = stats_for_window->global_req_size / stats_for_window->total_reqnb;

	if(!current_scheduler->needs_hashtable)
		timeline_lock();
	for(i=0; i< HASH_ENTRIES; i++)
	{
		if(current_scheduler->needs_hashtable)
			hashtable_lock(i);

		agios_list_for_each_entry(req_file, &hashtable[i], hashlist)
		{ 
			if(get_list_access_pattern(req_file->related_reads, RT_READ, &spatiality, &reqsize) == 0)//-1 means we dont have requests in this list
			{
				read_spatiality_count[spatiality]++;
				read_reqsize_count[reqsize]++;
			}
			if(get_list_access_pattern(req_file->related_writes, RT_WRITE, &spatiality, &reqsize) == 0)//-1 means we dont have requests in this list
			{
				write_spatiality_count[spatiality]++;
				write_reqsize_count[reqsize]++;
			}
		}

		if(current_scheduler->needs_hashtable)
			hashtable_unlock(i);
	}
	
	if(!current_scheduler->needs_hashtable)
		timeline_unlock();

	//we return the access pattern of the majority
	if(stats_for_window->writes >= stats_for_window->reads)
	{
		*global_operation = RT_WRITE;
		*global_spatiality = get_index_max(write_spatiality_count);
		*global_reqsize = get_index_max(write_reqsize_count);
	}
	else
	{
		*global_operation = RT_READ;
		*global_spatiality = get_index_max(read_spatiality_count);
		*global_reqsize = get_index_max(read_reqsize_count);
	}

	return 0;
}



void print_something(const char *something)
{
#ifdef AGIOS_KERNEL_MODULE
	seq_printf(stats_file,
#else
	fprintf(stats_file,
#endif
		"%s", something);
}

void print_selected_algs(void)
{
	int i;
	struct proc_alg_entry_t *entry;

	print_something("Scheduling algorithms:\n");

	agios_list_for_each_entry(entry, &proc_algs, list)
	{
#ifdef AGIOS_KERNEL_MODULE
		seq_printf(stats_file,
#else
		fprintf(stats_file,
#endif
			"%ld\t%s\n", entry->timestap, get_algorithm_name_from_index(entry->alg));
	}
	print_something("\n");
}

//start prints the initial information in the file and calls the other functions
#ifdef AGIOS_KERNEL_MODULE
void *print_stats_start(struct seq_file *s, loff_t *pos)
#else
void print_stats_start(void)
#endif
{
#ifdef AGIOS_KERNEL_MODULE
	stats_file = s;
#endif
	print_selected_algs();

	print_something("\t\treqs\tsize\tsize_avg\tsize_min\tsize_max\tsum_/_aggs\tagg_bigg\tagg_last\tshift\tbetter\tpdt_better\ttbr_avg\ttbr_min\ttbr_max\tproc_req\tband\n");

#ifdef AGIOS_KERNEL_MODULE
	hash_position=0;

	if(!current_scheduler->needs_hashtable)
		timeline_lock();
	return get_first(0);
#endif
}

#ifdef AGIOS_KERNEL_MODULE
void *print_predicted_stats_start(struct seq_file *s, loff_t *pos)
#else
void print_predicted_stats_start(void)
#endif
{

#ifdef AGIOS_KERNEL_MODULE
	seq_printf(s,
#else
	fprintf(stats_file,
#endif
	"\nPREDICTED_STATISTICS\n\ntracefile_counter: %d\nprediction_thread_init_time: %ld\nprediction_time_accepted_error: %d\ncurrent_predicted_reqfilenb: %d\n", get_predict_tracefile_counter(), get_predict_init_time(), config_predict_agios_time_error, get_current_predicted_reqfilenb());

	print_something("\t\treqs\tsize\tsize_avg\tsize_min\tsize_max\tsum_/_aggs\tagg_bigg\ttbr_avg\ttbr_min\ttbr_max\n");

#ifdef AGIOS_KERNEL_MODULE
	hash_position=0;
	return get_first(1);
#endif
}

void predicted_stats_show_related_list(struct related_list_t *related, const char *list_name)
{
	long int min_req_size, avg_req_size;
	double avg_agg_size;

	if(related->stats_file.min_req_size == LONG_MAX)
		min_req_size = 0;
	else
		min_req_size = related->stats_file.min_req_size;
	if(related->stats_file.processedreq_nb > 0)
		avg_req_size = (related->stats_file.total_req_size / related->stats_file.processedreq_nb);
	else
		avg_req_size = 0;
	if(related->stats_file.aggs_no > 0)
		avg_agg_size = (((double)related->stats_file.sum_of_agg_reqs)/((double)related->stats_file.aggs_no));
	else
		avg_agg_size = 1;

	if(strcmp(list_name, "read") == 0)
	{
#ifdef AGIOS_KERNEL_MODULE
		seq_printf(stats_file,
#else
		fprintf(stats_file,
#endif
		"file id: %s\n", related->req_file->file_id);
	}

#ifdef AGIOS_KERNEL_MODULE
	seq_printf(stats_file,
#else
	fprintf(stats_file,
#endif
	"\t%s:\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld/%ld=%.2f\t%d\t%Le\t%Le\n",
	   list_name,
	   related->stats_file.processedreq_nb,
	   related->stats_file.total_req_size,
	   avg_req_size,
	   min_req_size,
	   related->stats_file.max_req_size,
	   related->stats_file.sum_of_agg_reqs,
	   related->stats_file.aggs_no,
	   avg_agg_size,
	   related->best_agg,
	   related->stats_file.avg_distance,
	   related->avg_stripe_difference);

}

#ifdef AGIOS_KERNEL_MODULE
static int predicted_stats_show_one(struct seq_file *s, void *v)
#else
void predicted_stats_show_one(struct request_file_t *req_file)
#endif
{
#ifdef AGIOS_KERNEL_MODULE
	struct request_file_t *req_file = (struct request_file_t *)v;
	stats_file = s;
#endif
	if((req_file->predicted_reads.stats_file.processedreq_nb + req_file->predicted_writes.stats_file.processedreq_nb) <= 0)
#ifndef AGIOS_KERNEL_MODULE
			return;
#else
			return 0;
#endif

	predicted_stats_show_related_list(&req_file->predicted_reads, "read");
	predicted_stats_show_related_list(&req_file->predicted_writes, "write");

#ifdef AGIOS_KERNEL_MODULE
	return 0;
#endif
}

#ifndef AGIOS_KERNEL_MODULE
void stats_show_predicted(void)
{
	struct request_file_t *req_file;
	int i;
	struct agios_list_head *reqfile_l;

	for(i = 0; i < AGIOS_HASH_ENTRIES; i++)
	{
		reqfile_l = hashtable_lock(i);

		if(!agios_list_empty(reqfile_l))
		{
			agios_list_for_each_entry(req_file, reqfile_l, hashlist)
			{
				if((req_file->predicted_writes.stats_file.processedreq_nb > 0) || (req_file->predicted_reads.stats_file.processedreq_nb > 0))
					predicted_stats_show_one(req_file);
			}
		}

		hashtable_unlock(i);
	}
}
#endif

void stats_show_related_list(struct related_list_t *related, const char *list_name)
{
	long int min_request_time, avg_request_time;
	long int min_req_size, avg_req_size;
	double avg_agg_size;
	double bandwidth=0.0;
	long double tmp_time;

	if(related->stats_file.min_request_time == LONG_MAX)
		min_request_time = 0;
	else
		min_request_time = related->stats_file.min_request_time;

	if(related->stats_file.min_req_size == LONG_MAX)
		min_req_size = 0;
	else
		min_req_size = related->stats_file.min_req_size;
	if(related->stats_file.receivedreq_nb > 0)
		avg_req_size = (related->stats_file.total_req_size / related->stats_file.receivedreq_nb);
	else
		avg_req_size = 0;
	if(related->stats_file.aggs_no > 0)
		avg_agg_size = (((double)related->stats_file.sum_of_agg_reqs)/((double)related->stats_file.aggs_no));
	else
		avg_agg_size = 0;
	if(related->stats_file.receivedreq_nb > 1)
		avg_request_time = (related->stats_file.total_request_time / (related->stats_file.receivedreq_nb-1));
	else
		avg_request_time = 0;
	if(related->stats_file.processedreq_nb > 0)
	{
		tmp_time = get_ns2s(related->stats_file.processed_req_time);
		if(tmp_time > 0)
			bandwidth = ((related->stats_file.processed_req_size) / tmp_time) / 1024.0;
	}

	if(strcmp(list_name, "read") == 0)
	{
#ifndef AGIOS_KERNEL_MODULE
		fprintf(stats_file,
#else
		seq_printf(stats_file,
#endif
	   	"file id: %s\n", related->req_file->file_id);
	}

#ifndef AGIOS_KERNEL_MODULE
	fprintf(stats_file,
#else
	seq_printf(stats_file,
#endif
	   "\t%s:\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld/%ld=%.2f\t%u\t%u\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%.2f\n",
	   list_name,
	   related->stats_file.receivedreq_nb,
	   related->stats_file.total_req_size,
	   avg_req_size,
	   min_req_size,
	   related->stats_file.max_req_size,
	   related->stats_file.sum_of_agg_reqs,
	   related->stats_file.aggs_no,
	   avg_agg_size,
	   related->best_agg,
	   related->lastaggregation,
	   related->shift_phenomena,
	   related->better_aggregation,
	   related->predicted_better_aggregation,
           avg_request_time,
	   min_request_time,
	   related->stats_file.max_request_time,
	   related->stats_file.processedreq_nb,
	   bandwidth);

}

#ifdef AGIOS_KERNEL_MODULE
static int stats_show_one(struct seq_file *s, void *v)
#else
void stats_show_one(struct request_file_t *req_file)
#endif
{
#ifdef AGIOS_KERNEL_MODULE
	struct request_file_t *req_file = (struct request_file_t *)v;
	stats_file = s;
#endif
	if((req_file->related_reads.stats_file.receivedreq_nb + req_file->related_writes.stats_file.receivedreq_nb) <= 0)
#ifndef AGIOS_KERNEL_MODULE
			return;
#else
			return 0;
#endif
	stats_show_related_list(&req_file->related_reads, "read");
	stats_show_related_list(&req_file->related_writes, "write");

#ifdef AGIOS_KERNEL_MODULE
	return 0;
#endif
}

#ifdef AGIOS_KERNEL_MODULE
static void stats_show_ending(struct seq_file *s, void *v)
#else
void stats_show_ending(void)
#endif
{
	long int global_avg_time;
	long int global_avg_size;
	double *bandwidth;
	int i, len;
	char *band_str = malloc(sizeof(char)*50*config_agios_performance_values);

	agios_mutex_lock(&global_statistics_mutex);

	if(stats_for_file.total_reqnb > 1)
		global_avg_time = (stats_for_file.global_req_time / (stats_for_file.total_reqnb-1));
	else
		global_avg_time = 0;
	if(stats_for_file.total_reqnb > 0)
		global_avg_size = (stats_for_file.global_req_size / stats_for_file.total_reqnb);
	else
		global_avg_size = 0;



#ifndef AGIOS_KERNEL_MODULE
	fprintf(stats_file,
#else
	seq_printf(stats_file,
#endif
	"total of %ld requests\n\tavg\tmin\tmax\nglobal time between requests:\t%ld\t%ld\t%ld\nrequest size:\t%ld\t%ld\t%ld\n",
	stats_for_file.total_reqnb,
	global_avg_time,
	stats_for_file.global_min_req_time,
	stats_for_file.global_max_req_time,
	global_avg_size,
	stats_for_file.global_min_req_size,
	stats_for_file.global_max_req_size);

	agios_mutex_unlock(&global_statistics_mutex);

	bandwidth = agios_get_performance_bandwidth();
	band_str[0]='\n';
	band_str[1]='\0';
	i = performance_start;
	while(i != performance_end)
	{
		len = strlen(band_str);
		sprintf(band_str+len, "%.2f\n", bandwidth[i]/1024.0);
		i++;
		if(i >= config_agios_performance_values)
			i = 0;
	}
#ifndef AGIOS_KERNEL_MODULE
	fprintf(stats_file,
#else
	seq_printf(stats_file,
#endif
	"served an amount of %ld bytes in a total of %ld ns, bandwidth was:%s",
	agios_get_performance_size(),
	agios_get_performance_time(),
	band_str);


}

#ifndef AGIOS_KERNEL_MODULE
void stats_show(void)
{
	struct request_file_t *req_file;
	int i;
	struct agios_list_head *reqfile_l;

	if(!current_scheduler->needs_hashtable)
		timeline_lock();

	for(i = 0; i < AGIOS_HASH_ENTRIES; i++)
	{
		if(current_scheduler->needs_hashtable)
			reqfile_l = hashtable_lock(i);
		else
			reqfile_l = &hashlist[i];

		if(!agios_list_empty(reqfile_l))
		{
			agios_list_for_each_entry(req_file, reqfile_l, hashlist)
			{
				stats_show_one(req_file);
			}
		}
		if(current_scheduler->needs_hashtable)
			hashtable_unlock(i);
	}

	if(!current_scheduler->needs_hashtable)
		timeline_unlock();

	stats_show_ending();
}
#endif

void reset_stats_related_list(struct related_list_t *related_list)
{
	related_list->laststartoff = 0;
	related_list->lastfinaloff = 0;
	related_list->predictedoff = 0;

	related_list->lastaggregation = 0;
	related_list->best_agg = 0;

	related_list->shift_phenomena = 0;
	related_list->better_aggregation = 0;
	related_list->predicted_better_aggregation = 0;

	related_list->stats_file.processedreq_nb = 0;
	related_list->stats_file.receivedreq_nb = 0;
	related_list->stats_file.processed_req_size = 0;
	related_list->stats_file.processed_req_time = 0;

	related_list->stats_file.total_req_size=0;
	related_list->stats_file.min_req_size=LONG_MAX;
	related_list->stats_file.max_req_size=0;

	related_list->stats_file.max_request_time = 0;
	related_list->stats_file.total_request_time = 0;
	related_list->stats_file.min_request_time = LONG_MAX;

	related_list->stats_file.avg_distance=0;
	related_list->stats_file.avg_distance_count=1;

	related_list->stats_file.aggs_no = 0;
	related_list->stats_file.sum_of_agg_reqs = 0;

}

void agios_reset_stats()
{
	struct request_file_t *req_file;
	int i;
	struct agios_list_head *reqfile_l;

	reset_global_reqstats_file();
#ifndef AGIOS_KERNEL_MODULE
	if(config_trace_agios)
		agios_trace_reset();
#endif

	if(!current_scheduler->needs_hashtable)
		timeline_lock();

	for(i = 0; i < AGIOS_HASH_ENTRIES; i++)
	{
		if(current_scheduler->needs_hashtable)
			hashtable_lock(i);
		reqfile_l = &hashlist[i];

		if(!agios_list_empty(reqfile_l))
		{
			agios_list_for_each_entry(req_file, reqfile_l, hashlist)
			{
				reset_stats_related_list(&req_file->related_reads);
				reset_stats_related_list(&req_file->related_writes);

			}
		}
		if(current_scheduler->needs_hashtable)
			hashtable_unlock(i);
	}
	if(!current_scheduler->needs_hashtable)
		timeline_unlock();

}

#ifndef AGIOS_KERNEL_MODULE
void agios_print_stats_file(char *filename)
{
	PRINT_FUNCTION_NAME;

	/*opens the stats file*/
	if(!filename)
	{
		agios_print("AGIOS: No filename to stats file!\n");
		return;
	}
	stats_file = fopen(filename, "w");
	if(!stats_file)
	{
		agios_print("Cannot create %s stats file\n", filename);
		return;
	}
	else
	{
		/*we can't show stats if the prediction thread did not finished reading from trace files*/
		agios_wait_predict_init();
		/*prints the start of the file*/
		print_stats_start();

		/*prints the stats*/
		stats_show();

		print_predicted_stats_start();
		stats_show_predicted();

		/*closes the file*/
		fclose(stats_file);
	}

}
#endif

//register proc entries (useful only to the kernel module implementation)
//returns 0 on success
int proc_stats_init(void)
{
#ifdef AGIOS_KERNEL_MODULE
	struct proc_dir_entry *entry;

	agios_proc_dir = proc_mkdir("agios", NULL);
	if(!agios_proc_dir)
	{
		agios_print("Cannot create /proc/agios entry");
		return -ENOMEM;
	}

	entry = create_proc_entry("stats", 444, agios_proc_dir);
	if(entry)
		entry->proc_fops = &stats_proc_ops;

	entry = create_proc_entry("predicted_stats", 444, agios_proc_dir);
	if(entry)
		entry->proc_fops = &predicted_stats_proc_ops;

#ifdef AGIOS_DEBUG
	entry = create_proc_entry("hashtable", 444, agios_proc_dir);
	if(entry)
		entry->proc_fops = &hashtable_proc_ops;
#endif

#endif
	return 0;
}
/*unregister proc entries (useful only to the kernel module implementation) and free data structures*/
void proc_stats_exit(void)
{
	struct proc_alg_entry_t *entry, *aux=NULL;

#ifdef AGIOS_KERNEL_MODULE
	remove_proc_entry("stats", agios_proc_dir);
	remove_proc_entry("predicted_stats", agios_proc_dir);
#ifdef AGIOS_DEBUG
	remove_proc_entry("hashtable", agios_proc_dir);
#endif
	remove_proc_entry("agios", NULL);
#endif

	agios_list_for_each_entry(entry, &proc_algs, list)
	{
		if(aux)
		{
			agios_list_del(&aux->list);
			free(aux);
		}
		aux = entry;
	}
	if(aux)
	{
		agios_list_del(&aux->list);
		free(aux);
	}
}

/***********************************************************************************
 * functions to handle the proc entry - for the kernel module implementation only  *
 ***********************************************************************************/
#ifdef AGIOS_KERNEL_MODULE
static void *stats_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct request_file_t *req_file;

	(*pos)++;
	if (*pos >= current_reqfilenb)
		return NULL;

	req_file = (struct request_file_t *)v;

	if(req_file->hashlist.next == &hashlist[hash_position]) //we finished printing this hash position
	{
		if(current_scheduler->needs_hashtable)
			hashtable_unlock(hash_position);

		hash_position++;
		if(hash_position >= AGIOS_HASH_ENTRIES) //we are done with the whole hashtable
		{
			if(!current_scheduler->needs_hashtable)
				timeline_unlock();
			return NULL;
		}
		else
			return get_first(0);
		}
	}
	return agios_list_entry(req_file->hashlist.next, struct request_file_t, hashlist);
}
static void *predicted_stats_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct request_file_t *req_file;

	(*pos)++;
	if (*pos >= get_current_predicted_reqfilenb())
		return NULL;

	req_file = (struct request_file_t *)v;

	if(req_file->hashlist.next == &hashlist[hash_position]) //we finished printing this hash position
	{
		hashtable_unlock(hash_position);
		hash_position++;
		if(hash_position >= AGIOS_HASH_ENTRIES) //we are done with the whole hashtable
			return NULL;
		else
			return get_first(1);
	}
	return agios_list_entry(req_file->hashlist.next, struct request_file_t, hashlist);
}

static struct seq_operations stats_seq_ops = {
	.start		= print_stats_start,
	.next		= stats_seq_next,
	.stop		= stats_show_ending,
	.show		= stats_show_one
};

static int stats_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &stats_seq_ops);
}

static ssize_t stats_proc_write(struct file * file, const char __user * buf,
				    size_t count, loff_t *ppos)
{

	agios_reset_stats();
	return count;
}

static struct file_operations stats_proc_ops = {
	.owner		= THIS_MODULE,
	.open		= stats_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
	.write		= stats_proc_write,
};


static void predicted_stats_stop(struct seq_file *s, void *v)
{
}

static struct seq_operations predicted_stats_seq_ops = {
	.start		= print_predicted_stats_start,
	.next		= predicted_stats_seq_next,
	.stop		= predicted_stats_stop,
	.show		= predicted_stats_show_one
};

static int predicted_stats_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &predicted_stats_seq_ops);
}

static ssize_t predicted_stats_proc_write(struct file * file, const char __user * buf,
				    size_t count, loff_t *ppos)
{
	return count;
}

static struct file_operations predicted_stats_proc_ops = {
	.owner		= THIS_MODULE,
	.open		= predicted_stats_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
	.write		= predicted_stats_proc_write,
};

#ifdef AGIOS_DEBUG
static int hashtable_proc_index=0;
void *hashtable_start(struct seq_file *s, loff_t *pos)
{
	hashtable_proc_index=0;
	if(current_scheduler->needs_hashtable)
		hashtable_lock(hashtable_proc_index);
	else
		timeline_lock();
	return &hashlist[hashtable_proc_index];
}

static int hashtable_show(struct seq_file *s, void *v)
{
	struct request_file_t *req_file;
	struct agios_list_head *hash_list = (struct agios_list_head *) v;
	struct request_t *tmp;

	seq_printf(s, "[%d]\n", hashtable_proc_index);

	if(agios_list_empty(hash_list))
		return 0;

	agios_list_for_each_entry(req_file, hash_list, hashlist) {
		seq_printf(s, "     [%s]:", req_file->file_id);
		seq_printf(s, "\n\t reads : ");
		agios_list_for_each_entry(tmp, &req_file->related_reads.list, related) {
			seq_printf(s, "(%d; %d), ", (int)tmp->io_data.offset,
			                   	    (int)tmp->io_data.offset +
	   			        	    (int)tmp->io_data.len);
		}
		seq_printf(s, "\n\t writes: ");

		agios_list_for_each_entry(tmp, &req_file->related_writes.list, related) {
			seq_printf(s, "(%d; %d), ", (int)tmp->io_data.offset,
			       			    (int)tmp->io_data.offset +
			       			    (int)tmp->io_data.len);
		}
		seq_printf(s, "\n");
	}
	if(current_scheduler->needs_hashtable)
		hashtable_unlock(hashtable_proc_index);
	else
		timeline_unlock();
	return 0;
}

static void *hashtable_seq_next(struct seq_file *s, void *v, loff_t *pos)

{
	if(current_scheduler->needs_hashtable)
	{
		hashtable_proc_index++;
		return hashtable_lock(hashtable_proc_index);
	}
	else
		return NULL;
}

static void hashtable_stop(struct seq_file *s, void *v)
{
	hashtable_proc_index=0;
}

static struct seq_operations hashtable_seq_ops = {
	.start		= hashtable_start,
	.next		= hashtable_seq_next,
	.stop		= hashtable_stop,
	.show		= hashtable_show
};

static int hashtable_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &hashtable_seq_ops);
}

static ssize_t hashtable_proc_write(struct file * file, const char __user * buf,
				    size_t count, loff_t *ppos)
{
	return count;
}

static struct file_operations hashtable_proc_ops = {
	.owner		= THIS_MODULE,
	.open		= hashtable_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
	.write		= hashtable_proc_write,
};

#endif /* #ifdef AGIOS_DEBUG */


#endif /* #ifdef AGIOS_KERNEL_MODULE */


