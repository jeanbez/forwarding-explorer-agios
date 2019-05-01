#ifndef _REQ_HASHTABLE_H_
#define _REQ_HASHTABLE_H_

#include "agios_request.h"

#define AGIOS_HASH_SHIFT 6						
#define AGIOS_HASH_ENTRIES		(1 << AGIOS_HASH_SHIFT) 		

/*init and exit functions*/
int hashtable_init(void);
void hashtable_cleanup(void);

/* to requests management */
void hashtable_add_req(struct request_t *req, long hash_val, struct request_file_t *given_req_file);
void __hashtable_del_req(struct request_t *req);
void hashtable_del_req(struct request_t *req);

/*to access the hashtable from outside*/
extern struct agios_list_head *hashlist;
extern int *hashlist_reqcounter;
struct agios_list_head *hashtable_lock(int index);
struct agios_list_head *hashtable_trylock(int index);
void hashtable_unlock(int index);
#endif
