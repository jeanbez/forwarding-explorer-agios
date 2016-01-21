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

#ifdef AGIOS_KERNEL_MODULE
#include <linux/completion.h>
#else
#include <pthread.h>
#include <stdint.h>
#endif

/*
 * Request type.
 */
enum {
	RT_READ = 0,
	RT_WRITE = 1,
};

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


#endif // #ifndef AGIOS_H
