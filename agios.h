/*! \file agios.h
    \brief TODO 

    TODO a detailed description
*/


#pragma once AGIOS_H

#include <pthread.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
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
int agios_init(struct client *clnt, char *config_file, int max_app_id); //max_app_id is only relevant for exclusive time window scheduler, otherwise you MUST provide 0. Also we will assume app_id will always be something between 0 and max_app_id. 
void agios_exit(void);


#ifdef ORANGEFS_AGIOS
int agios_add_request(char *file_id, short int type, long int offset,
		       long int len, int64_t data, struct client *clnt, int app_id);
#else
int agios_add_request(char *file_id, short int type, long int offset,
		       long int len, void * data, struct client *clnt, int app_id);
#endif
int agios_release_request(char *file_id, short int type, long int len, long int offset); //returns 1 on success

int agios_set_stripe_size(char *file_id, int stripe_size);

int agios_cancel_request(char *file_id, short int type, long int len, long int offset);

void agios_print_stats_file(char *filename);
void agios_reset_stats(void);

void agios_wait_predict_init(void);

#ifdef __cplusplus
}
#endif

