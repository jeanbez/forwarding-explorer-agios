/* File:	req_timeline.c
 * Created: 	September 2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 * Collaborators:
 * 		Jean Luca Bez <jlbez (at) inf.ufrgs.br>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It implements the timeline data structure used by some schedulers to keep requests
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
#ifndef _REQ_TIMELINE_H_
#define _REQ_TIMELINE_H_

#include "agios_request.h"

/* to access the associated data structures*/
extern struct agios_list_head timeline;
struct agios_list_head *timeline_lock(void);
void timeline_unlock(void);

/* to include or remove requests*/
void timeline_add_req(struct request_t *req, unsigned long hash, struct request_file_t *given_req_file);
void __timeline_add_req(struct request_t *req, unsigned long hash_val, struct request_file_t *given_req_file, struct agios_list_head *this_timeline);
struct request_t *timeline_oldest_req(unsigned long *hash);

/* timeline management functions*/
void reorder_timeline();
void timeline_init();
inline void timeline_cleanup();

#endif
