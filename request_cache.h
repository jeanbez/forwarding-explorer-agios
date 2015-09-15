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
struct request_file_t;

/* to access the counters*/
inline int get_current_reqnb();
inline int get_current_reqfilenb();
inline void set_current_reqnb(int value);
inline void set_current_reqfilenb(int value);
inline void inc_current_reqnb();
inline void dec_current_reqnb(int hash);
inline void dec_many_current_reqnb(int hash, int value);
inline void inc_current_reqfilenb();
inline void dec_current_reqfilenb();

/* to set the local copies of configuration file parameters */
inline void set_request_cache_predict_request_aggregation(short int value);
inline void set_request_cache_trace(short int value);

/* to access the scheduling algorithms management parameters and functions */
inline int get_selected_alg();
inline void set_selected_alg(int value); //use it only at first, when setting the parameters. DON'T use it for changing the current scheduling algorithm (use change_selected_alg instead)
inline void set_max_aggregation_size(int value); //use it only at first, when setting the parameters. DON'T use it for changing the current scheduling algorithm (use change_selected_alg instead)
inline int get_max_aggregation_size();
inline short int get_needs_hashtable();
inline void set_needs_hashtable(short int value);
void change_selected_alg(int new_alg, short int new_needs_hashtable, int new_max_aggregation_size);
inline void unlock_algorithm_migration_mutex(void);
inline void lock_algorithm_migration_mutex(void);

/* data structures management functions */
void request_cache_free(struct request_t *req);
#ifdef ORANGEFS_AGIOS
struct request_t * request_constructor(char *file_id, int type, long long offset, long len, int64_t data, unsigned long long int arrival_time, int state);
#else
struct request_t * request_constructor(char *file_id, int type, long long offset, long len, int data, unsigned long long int arrival_time, int state);
#endif
struct request_file_t * request_file_constructor(char *file_id);

/* init and exit functions*/
int request_cache_init(void);
void request_cache_cleanup(void);

/* to add new requests */
int insert_aggregations(struct request_t *req, struct agios_list_head *insertion_place, struct agios_list_head *list_head);
void include_in_aggregation(struct request_t *req, struct request_t **agg_req);
struct request_file_t *find_req_file(struct agios_list_head *hash_list, char *file_id, int state);
#ifdef ORANGEFS_AGIOS
int agios_add_request(char *file_id, int type, long long offset, long len, int64_t data, struct client *clnt);
#else
int agios_add_request(char *file_id, int type, long long offset, long len, int data, struct client *clnt); //TODO we will need app_id for time_window as well
#endif
#endif // #ifndef  REQUEST_CACHE_H
