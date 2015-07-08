/* File:	iosched.c
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the implementation for the five scheduling algorithms. 
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

int get_algorithm_from_string(const char *alg);
char *get_algorithm_name_from_index(int index);

/*
 * Structure describing I/O scheduler operations.
 */
struct io_scheduler_instance_t {

	struct agios_list_head list;
	
	/*
	 * Called when new consumer thread using this scheduler is started.
	 * Intended to allow I/O scheduler to initialized it's private data.
	 */
	int (*init)(void);	

	/*
	 * Called when scheduler is no longer use by some thread, but remember
	 * that it can be still used by other threads.
	 */
	void (*exit)(void);
	
	/*
	 * This is the main scheduler function. 
	 */
	void (*schedule)(void *clnt);

	/*parameters*/
	short int sync; //should we sync after each request (wait until the client finished processing it)
	short int needs_hashtable; //if 0, then timeline is used for requests
	int max_aggreg_size; //maximum number of requests to be aggregated at once
	char name[20];
	int index;

};

/* Prototypes */
void register_static_io_schedulers(void);
struct io_scheduler_instance_t *find_io_scheduler(int index);
int initialize_scheduler(int index, void *consumer);
void setFS_BANDWIDTH(unsigned long value);

inline unsigned long long int get_time_spent_waiting(void);
inline unsigned long long int get_waiting_time_overlapped(void);


/*TODO maybe we could tolerate some useless space between the two aggregated requests, since it is possible that, because of block sizes, this useless portion will be requested anyway*/
#define CHECK_AGGREGATE(req,nextreq) \
     ( (req->io_data.offset <= nextreq->io_data.offset)&& \
         ((req->io_data.offset+req->io_data.len)>=nextreq->io_data.offset))

#endif // #ifndef IOSCHED_H
