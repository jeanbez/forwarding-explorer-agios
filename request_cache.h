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


struct request_t;

void timeline_add_req(struct request_t *req);

/*
 * WARNING: these two functions delete request from list before
 * returning it.
 */
struct request_t *timeline_oldest_req(void);
struct request_t *timeline_newest_req(void);

void timeline_del_req(struct request_t *req);
int timeline_empty(void);

struct agios_list_head *timeline_lock(void);
void timeline_unlock(void);
struct agios_list_head *get_timeline_files(void);

void request_cache_free(struct request_t *req);
int request_cache_init(void);
void request_cache_cleanup(void);

unsigned long hashtable_add_req(struct request_t *req);
void hashtable_del_req(struct request_t *req);
void __hashtable_del_req(struct request_t *req);

struct agios_list_head *hashtable_lock(int index);
struct agios_list_head *hashtable_trylock(int index);
void hashtable_unlock(int index);
int hashtable_get_size(void);

#ifdef ORANGEFS_AGIOS
struct request_t * request_constructor(char *file_id, int type, long long offset, long len, int64_t data, unsigned long long int arrival_time, int state);
#else
struct request_t * request_constructor(char *file_id, int type, long long offset, long len, int data, unsigned long long int arrival_time, int state);
#endif

struct request_file_t * request_file_constructor(char *file_id);

inline int get_current_reqnb(void);
inline int get_current_reqfilenb(void);
inline void set_current_reqnb(int value);
inline void set_current_reqfilenb(int value);
inline void inc_current_reqnb(void);
inline void inc_current_reqfilenb(void);
inline void dec_current_reqfilenb(void);
inline void dec_current_reqnb(int hash);
inline void dec_many_current_reqnb(int hash, int value);

inline int get_hashlist_reqcounter(int hash);
inline void set_max_aggregation_size(int value);
inline void set_needs_hashtable(short int value);
inline short int get_needs_hashtable(void);

inline struct request_file_t *find_req_file(struct agios_list_head *hash_list, char *file_id, int state);
struct agios_list_head *get_hashlist(int index);
#endif // #ifndef  REQUEST_CACHE_H
