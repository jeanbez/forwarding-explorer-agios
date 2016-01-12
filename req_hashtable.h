/* File:	req_hashtable.h
 * Created: 	September 2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 * Collaborators:
 * 		Jean Luca Bez <jlbez (at) inf.ufrgs.br>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It implements the hashtable data structure used by some schedulers to keep requests.
 *		Further information is available at http://agios.bitbucket.org/
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

#ifndef _REQ_HASHTABLE_H_
#define _REQ_HASHTABLE_H_

/*init and exit functions*/
int hashtable_init(void);
void hashtable_cleanup(void);

/* to requests management */
void hashtable_add_req(struct request_t *req, unsigned long hash_val, struct request_file_t *given_req_file);
void __hashtable_del_req(struct request_t *req);
void hashtable_del_req(struct request_t *req);

/*to access the hashtable from outside*/
extern struct agios_list_head *hashlist;
extern int *hashlist_reqcounter;
struct agios_list_head *hashtable_lock(int index);
struct agios_list_head *hashtable_trylock(int index);
void hashtable_unlock(int index);
#endif
