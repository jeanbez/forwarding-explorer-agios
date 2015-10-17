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
#include "agios.h"
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

#ifdef AGIOS_KERNEL_MODULE
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#else
#include <string.h>
#endif

/***********************************************************************************************************
 * GLOBAL ACCESS PATTERN STATISTICS *
 ***********************************************************************************************************/
struct global_statistics_t 
{ 
	unsigned long int total_reqnb; //we have a similar counter in consumer.c, but this one can be reset, that one is fixed (never set to 0, counts through the whole execution)
//to measure time between requests
	unsigned long long int global_req_time; 
	unsigned long int global_min_req_time;
	unsigned long int global_max_req_time;
	//to measure request size
	unsigned long long int global_req_size;
	unsigned long int global_min_req_size; 
	unsigned long int global_max_req_size;
};
static struct timespec last_req; 
static struct global_statistics_t stats_for_file;
static struct global_statistics_t stats_for_window;
static pthread_mutex_t global_statistics_mutex = PTHREAD_MUTEX_INITIALIZER;



/***********************************************************************************************************
 * LOCAL COPIES OF PARAMETERS *
 ***********************************************************************************************************/
static short int proc_needs_hashtable=1;
static int *proc_algs; //the list of the last PROC_ALGS_SIZE selected scheduling algorithms
static unsigned long int *proc_algs_timestamps; //the timestamps of the last PROC_ALGS_SIZE algorithms selections
#define PROC_ALGS_SIZE 1000 //how many should we keep (we actually keep PROC_ALGS_SIZE - 1)
static int proc_algs_start, proc_algs_end=0; //indexes to access the proc_algs list 

inline void proc_set_needs_hashtable(short int value)
{
	proc_needs_hashtable=value;
}
inline void proc_set_new_algorithm(int alg)
{
	struct timespec now;

	agios_gettime(&now);
	proc_algs[proc_algs_end] = alg;
	proc_algs_timestamps[proc_algs_end] = get_timespec2llu(now);
	proc_algs_end++;
	if(proc_algs_end >= PROC_ALGS_SIZE)
		proc_algs_end=0; //circular list
	if(proc_algs_start == proc_algs_end)
	{
		proc_algs_start++; 	//we remove the oldest
		if(proc_algs_start >= PROC_ALGS_SIZE)
			proc_algs_start = 0;
	}
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
	unsigned long int elapsedtime=0;
	long int this_distance;

	//update local statistics on time between requests
	if(stats->total_request_time == 0)
		stats->total_request_time = 1;
	else
	{
		elapsedtime = (req->jiffies_64 - get_timespec2llu(req->globalinfo->last_req_time))/1000; //nano to microseconds
		if(stats->total_request_time == 1)
			stats->total_request_time = 0;

		stats->total_request_time += elapsedtime;
		
		if(elapsedtime > stats->max_request_time)
			stats->max_request_time = elapsedtime;
		if(elapsedtime < stats->min_request_time)
			stats->min_request_time = elapsedtime;
	}
	get_llu2timespec(req->jiffies_64, &req->globalinfo->last_req_time);
		
	//update local statistics on average offset distance between consecutive requests
	//TODO is it the same thing done by predict when including requests? should we join these codes?
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
void update_global_stats_newreq(struct global_statistics_t *stats, struct request_t *req, unsigned long int elapsedtime)
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
} 
/*update the stats after the arrival of a new request
 * must hold the hashtable mutex 
 */
void proc_stats_newreq(struct request_t *req)
{
	unsigned long int elapsedtime=0;

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
			elapsedtime = (req->jiffies_64 - get_timespec2llu(last_req)) / 1000; //nano to microseconds
		update_global_stats_newreq(&stats_for_file, req, elapsedtime); 
		update_global_stats_newreq(&stats_for_window, req, elapsedtime); 
		get_llu2timespec(req->jiffies_64, &last_req);

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
	stats->global_min_req_time = ~0;
	stats->global_max_req_time = 0;
	stats->global_req_size = 0;
	stats->global_min_req_size = ~0;
	stats->global_max_req_size=0;
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
	related_list->stats_window.min_req_size=~0;
	related_list->stats_window.max_req_size=0;

	related_list->stats_window.max_request_time = 0;
	related_list->stats_window.total_request_time = 0;
	related_list->stats_window.min_request_time = ~0;

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

	if(proc_needs_hashtable)
	{
		for(i=0; i< AGIOS_HASH_ENTRIES; i++)
		{
			list = get_hashlist(i);
			agios_list_for_each_entry(req_file, list, hashlist)
			{
				reset_stats_window_related_list(&req_file->related_reads);
				reset_stats_window_related_list(&req_file->related_writes);
			}
		}
	}
	else
	{
		list = get_timeline_files();
		agios_list_for_each_entry(req_file, list, hashlist)
		{
			reset_stats_window_related_list(&req_file->related_reads);
			reset_stats_window_related_list(&req_file->related_writes);
		}
	}
	reset_global_reqstats_window();
	reset_performance_counters();
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
		if((proc_needs_hashtable) || (predicted_stats))
			reqfile_l = hashtable_lock(hash_position);	
		else
		{
			timeline_lock();
			reqfile_l = get_timeline_files();
		}

		if(!agios_list_empty(reqfile_l))
		{ //take the first element of the list
			agios_list_for_each_entry(req_file, reqfile_l, hashlist)
				break;
		}	
		else
		{//list is empty, nothing to show, unlock the mutex
			if((proc_needs_hashtable) || (predicted_stats))
			{
				hashtable_unlock(hash_position);
				hash_position++;
				if(hash_position >= AGIOS_HASH_ENTRIES)
					break;
			}
			else
			{
				timeline_unlock();
				break;
			}
		}
	}
	return req_file;	
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

	print_something("Scheduling algorithms:\n");
	i = proc_algs_start;
	while(i != proc_algs_end)
	{
#ifdef AGIOS_KERNEL_MODULE
		seq_printf(stats_file, 
#else
		fprintf(stats_file, 
#endif
			"%lu\t%s\n", proc_algs_timestamps[i], get_algorithm_name_from_index(proc_algs[i]));
		i++;
		if(i >= PROC_ALGS_SIZE)
			i =0;
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
	"\nPREDICTED_STATISTICS\n\ntracefile_counter: %d\nprediction_thread_init_time: %llu\nprediction_time_accepted_error: %d\ncurrent_predicted_reqfilenb: %d\n", get_predict_tracefile_counter(), get_predict_init_time(), config_get_prediction_time_error(), get_current_predicted_reqfilenb());
	
	print_something("\t\treqs\tsize\tsize_avg\tsize_min\tsize_max\tsum_/_aggs\tagg_bigg\ttbr_avg\ttbr_min\ttbr_max\n");

#ifdef AGIOS_KERNEL_MODULE
	hash_position=0;
	return get_first(1); 
#endif
}

void predicted_stats_show_related_list(struct related_list_t *related, const char *list_name)
{
	unsigned long int min_req_size, avg_req_size;
	double avg_agg_size;

	if(related->stats_file.min_req_size == ~0)
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
	"\t%s:\t%lu\t%llu\t%lu\t%lu\t%lu\t%lu/%lu=%.2f\t%u\t%Le\t%Le\n",
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
	unsigned long int min_request_time, avg_request_time;
	unsigned long int min_req_size, avg_req_size;
	double avg_agg_size;
	double bandwidth=0.0;
	long double tmp_time;

	if(related->stats_file.min_request_time == ~0)
		min_request_time = 0;
	else
		min_request_time = related->stats_file.min_request_time;

	if(related->stats_file.min_req_size == ~0)
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
	   "\t%s:\t%lu\t%llu\t%lu\t%lu\t%lu\t%lu/%lu=%.2f\t%u\t%u\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%.2f\n",
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
	unsigned long int global_avg_time;
	unsigned long int global_avg_size;

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
	"total of %lu requests\n\tavg\tmin\tmax\nglobal time between requests:\t%lu\t%lu\t%lu\nrequest size:\t%lu\t%lu\t%lu\n", 
	stats_for_file.total_reqnb, 
	global_avg_time,
	stats_for_file.global_min_req_time,
	stats_for_file.global_max_req_time,
	global_avg_size,
	stats_for_file.global_min_req_size,
	stats_for_file.global_max_req_size);

	agios_mutex_unlock(&global_statistics_mutex);

#ifndef AGIOS_KERNEL_MODULE
	fprintf(stats_file,  
#else
	seq_printf(stats_file,
#endif
	"served an amount of %llu bytes, bandwidth was %.2f KB/s",
	get_performance_size(),
	get_performance_bandwidth()/1024.0);


}

#ifndef AGIOS_KERNEL_MODULE
void stats_show(void)
{
	struct request_file_t *req_file;
	int i;
	struct agios_list_head *reqfile_l;

	if(proc_needs_hashtable)
	{
		for(i = 0; i < AGIOS_HASH_ENTRIES; i++)
		{
			reqfile_l = hashtable_lock(i);
			
			if(!agios_list_empty(reqfile_l))
			{
				agios_list_for_each_entry(req_file, reqfile_l, hashlist)
				{
					stats_show_one(req_file);
				}
			}		

			hashtable_unlock(i);
		}
	}
	else
	{
		debug("printing statistics to file from timeline");
		timeline_lock();
		reqfile_l = get_timeline_files();
		if(!agios_list_empty(reqfile_l))
		{
			agios_list_for_each_entry(req_file, reqfile_l, hashlist)
			{
				stats_show_one(req_file);
			}
		}
		timeline_unlock();
	}

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
	related_list->stats_file.min_req_size=~0;
	related_list->stats_file.max_req_size=0;

	related_list->stats_file.max_request_time = 0;
	related_list->stats_file.total_request_time = 0;
	related_list->stats_file.min_request_time = ~0;

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
	if(config_get_trace())
		agios_trace_reset();	
#endif

	for(i = 0; i < AGIOS_HASH_ENTRIES; i++)
	{
		if(proc_needs_hashtable)
			reqfile_l = hashtable_lock(i);
		else
		{
			timeline_lock();
			reqfile_l = get_timeline_files();
		}
		
		if(!agios_list_empty(reqfile_l))
		{
			agios_list_for_each_entry(req_file, reqfile_l, hashlist)
			{
				reset_stats_related_list(&req_file->related_reads);
				reset_stats_related_list(&req_file->related_writes);

			}
		}
		if(proc_needs_hashtable)
			hashtable_unlock(i);
		else
		{
			timeline_unlock();
			break;
		}
	}
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

/*register proc entries (useful only to the kernel module implementation) and some other structures*/
void proc_stats_init(void)
{
	proc_algs = agios_alloc(sizeof(int)*(PROC_ALGS_SIZE+1));
	proc_algs_timestamps = agios_alloc(sizeof(unsigned long int)*(PROC_ALGS_SIZE+1));
	proc_algs_start = proc_algs_end = 0;
	
#ifdef AGIOS_KERNEL_MODULE
	struct proc_dir_entry *entry;

	agios_proc_dir = proc_mkdir("agios", NULL);
	if(!agios_proc_dir)
	{
		agios_print("Cannot create /proc/agios entry");
		return;
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
}
/*unregister proc entries (useful only to the kernel module implementation) and free data structures*/
void proc_stats_exit(void)
{
#ifdef AGIOS_KERNEL_MODULE
	remove_proc_entry("stats", agios_proc_dir);
	remove_proc_entry("predicted_stats", agios_proc_dir);
#ifdef AGIOS_DEBUG
	remove_proc_entry("hashtable", agios_proc_dir);
#endif
	remove_proc_entry("agios", NULL);	
#endif
	free(proc_algs);
	free(proc_algs_timestamps);
}

/***********************************************************************************
 * functions to handle the proc entry - for the kernel module implementation only  *
 ***********************************************************************************/
#ifdef AGIOS_KERNEL_MODULE
static void *stats_seq_next(struct seq_file *s, void *v, loff_t *pos) 
{
	struct request_file_t *req_file;
	
	(*pos)++;
	if (*pos >= get_current_reqfilenb()) 
		return NULL;
	
	req_file = (struct request_file_t *)v;

	if(proc_needs_hashtable)
	{
		if(req_file->hashlist.next == get_hashlist(hash_position)) //we finished printing this hash position
		{
			hashtable_unlock(hash_position);
			hash_position++;
			if(hash_position >= AGIOS_HASH_ENTRIES) //we are done with the whole hashtable
				return NULL;
			else
				return get_first(0);
		}
	}
	else
	{
		if(req_file->hashlist.next == get_timeline_files()) //we finished printing all files from the timeline
		{
			timeline_unlock();
			return NULL;
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

	if(req_file->hashlist.next == get_hashlist(hash_position)) //we finished printing this hash position
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
	if(proc_needs_hashtable)
		return hashtable_lock(hashtable_proc_index);
	else 
	{
		timeline_lock();
		return get_timeline_files();  
	}
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
	if(proc_needs_hashtable)
		hashtable_unlock(hashtable_proc_index);	
	else
		timeline_unlock();
	return 0;
}

static void *hashtable_seq_next(struct seq_file *s, void *v, loff_t *pos) 

{
	if(proc_needs_hashtable)
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


