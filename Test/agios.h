/*
 * created by Francieli Zanon Boito in 2012
 * updated by Jean Luca Bez 2015
 * interface of the library version of the aIOLi scheduler
 */

#ifndef AGIOS_H
#define AGIOS_H

#include <pthread.h>

#include <stdint.h>

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
    void (*process_request)(int req_id);
    void (*process_requests)(int *reqs, int reqnb);
};

int agios_init(struct client *clnt, char *config_file);
void agios_exit(void);

int agios_add_request(char *file_id, int type, long long offset, long len, int data, struct client *clnt);

void agios_print_stats_file(char *filename);
void agios_reset_stats(void);

#endif // #ifndef AGIOS_H
