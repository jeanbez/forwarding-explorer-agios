/* File:	proc.h
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It handles scheduling statistics and print them whenever the agios_print_stats_file 
 *		function is called. Stats can be reseted by agios_reset_stats.
 *		The kernel module implementation uses the proc interface, while the user-level
 *		library creates a file in the local file system. Its name is provided as a parameter.
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


#ifndef PROC_H
#define PROC_H

void stats_aggregation(struct related_list_t *related);
void stats_shift_phenomenon(struct related_list_t *related);
void stats_better_aggregation(struct related_list_t *related);
void reset_global_reqstats(void);
void stats_predicted_better_aggregation(struct related_list_t *related);
void proc_stats_newreq(struct request_t *req);

void reset_stats_window(void);

void proc_stats_init(void);
void proc_stats_exit(void);

void proc_set_needs_hashtable(short int value);
inline void proc_set_new_algorithm(int alg);
inline unsigned long int *proc_get_alg_timestamps(int *start, int *end);
#endif
