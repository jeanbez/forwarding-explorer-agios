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
void reset_performance_counters(void);
unsigned long long int get_performance_size(void);
unsigned long long int get_performance_time(void);
double *get_performance_bandwidth(int *start, int *end, int **algs);
double get_current_performance_bandwidth(void);
inline void performance_set_needs_hashtable(short int value);
void performance_set_new_algorithm(int alg);
#endif
