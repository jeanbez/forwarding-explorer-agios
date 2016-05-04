/* File:	request_cache.h
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides and handles the data structures to store files' and requests' 
 *		information.
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


#ifndef REQUEST_CACHE_H
#define REQUEST_CACHE_H

#include "mylist.h"

struct request_t;
struct request_file_t;

/* to access the counters*/
extern int current_reqnb;
extern int current_reqfilenb;
extern pthread_mutex_t current_reqnb_lock;
inline int get_current_reqnb(void); //this version uses the mutex to access the current_reqnb variable, use only if critical to have updated version
inline void inc_current_reqnb(void);
inline void dec_current_reqnb(int hash);
inline void dec_many_current_reqnb(int hash, int value);
inline void inc_current_reqfilenb(void);
inline void dec_current_reqfilenb(void);

/* data structures management functions */
void lock_all_data_structures();
void unlock_all_data_structures();
void request_cache_free(struct request_t *req);
#ifdef ORANGEFS_AGIOS
struct request_t * request_constructor(char *file_id, short int type, unsigned long int offset, unsigned long int len, int64_t data, unsigned long int arrival_time, short int state, unsigned int app_id);
#else
struct request_t * request_constructor(char *file_id, short int type, unsigned long int offset, unsigned long int len, void * data, unsigned long int arrival_time, short int state, unsigned int app_id); 
#endif
struct request_file_t * request_file_constructor(char *file_id);
void migrate_from_hashtable_to_timeline();
void migrate_from_timeline_to_hashtable();

/* for debug */
void print_hashtable(void);
inline void print_hashtable_line(int i);
void print_timeline(void);
inline void print_request(struct request_t *req);

/* init and exit functions*/
int request_cache_init(int max_app_id);
void request_cache_cleanup(void);
void list_of_requests_cleanup(struct agios_list_head *list);

/* to add new requests */
int insert_aggregations(struct request_t *req, struct agios_list_head *insertion_place, struct agios_list_head *list_head);
void include_in_aggregation(struct request_t *req, struct request_t **agg_req);
struct request_file_t *find_req_file(struct agios_list_head *hash_list, char *file_id, int state);
#endif // #ifndef  REQUEST_CACHE_H
