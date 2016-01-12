/* File:	performance.h
 * Created: 	2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 * Collaborators:
 *		Jean Luca Bez <jlbez (at) inf.ufrgs.br>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the release function, called by the user after processing requests.
 *		It keeps performance measurements and handles the synchronous approach.
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
#ifndef _PERFORMANCE_H_
#define _PERFORMANCE_H_

#define PERFORMANCE_VALUES 5

extern unsigned long int agios_processed_reqnb;

extern int performance_algs[PERFORMANCE_VALUES];
extern int performance_start, performance_end;
void agios_reset_performance_counters(void);
unsigned long long int agios_get_performance_size(void);
unsigned long long int agios_get_performance_time(void);
double *agios_get_performance_bandwidth();
double agios_get_current_performance_bandwidth(void);
inline void performance_set_needs_hashtable(short int value);
void performance_set_new_algorithm(int alg);
inline int agios_performance_get_latest_index();
#endif
