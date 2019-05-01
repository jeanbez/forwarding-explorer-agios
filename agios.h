/*! \file agios.h
    \brief TODO 

    TODO a detailed description
*/


#pragma once 

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
//TODO see how to document enum

//TODO ORANGEFS_AGIOS (use int64_t instead of void *)

bool agios_init(void * process_request(void * req_id), 
		void * process_requests(void **reqs, int32_t reqnb), 
		char *config_file, 
		int32_t max_queue_id);
void agios_exit(void);
bool agios_add_request(char *file_id, 
			int32_t type, 
			int64_t offset, 
			int64_t len, 
			int64_t identifier, 
			int32_t queue_id);
int agios_release_request(char *file_id, short int type, long int len, long int offset); //returns 1 on success

int agios_set_stripe_size(char *file_id, int stripe_size);

int agios_cancel_request(char *file_id, short int type, long int len, long int offset);

void agios_print_stats_file(char *filename);
void agios_reset_stats(void);


#ifdef __cplusplus
}
#endif

