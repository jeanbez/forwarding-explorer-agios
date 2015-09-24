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

/* to access the associated data structures*/
struct agios_list_head *timeline_lock(void);
void timeline_unlock(void);
struct agios_list_head *get_timeline_files(void);
struct agios_list_head *get_timeline(void);

/* to include or remove requests*/
void timeline_add_req(struct request_t *req, int max_aggregation_size, int selected_alg, struct request_file_t *given_req_file);
void __timeline_add_req(struct request_t *req, int max_aggregation_size, int selected_alg, struct request_file_t *given_req_file, struct agios_list_head *this_timeline);
struct request_t *timeline_oldest_req(void);

/* timeline management functions*/
void reorder_timeline(int new_alg, int new_max_aggregation_size);
void timeline_init();

#endif
