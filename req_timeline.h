#ifndef _REQ_TIMELINE_H_
#define _REQ_TIMELINE_H_

#include "agios_request.h"

/* to access the associated data structures*/
extern struct agios_list_head timeline;
extern struct agios_list_head *app_timeline;
extern int app_timeline_size;
struct agios_list_head *timeline_lock(void);
void timeline_unlock(void);

/* to include or remove requests*/
void timeline_add_req(struct request_t *req, long hash, struct request_file_t *given_req_file);
void __timeline_add_req(struct request_t *req, long hash_val, struct request_file_t *given_req_file, struct agios_list_head *this_timeline);
struct request_t *timeline_oldest_req(long *hash);

/* timeline management functions*/
void reorder_timeline();
void timeline_init();
void timeline_cleanup();

#endif
