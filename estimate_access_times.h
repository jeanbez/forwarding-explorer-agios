/* File:	estimate_access_times.h
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides an interface to obtain measured read/write processing
 *		time for different request sizes. It also provides the sequential
 *		to random throughput ratio. 
 *		Further information is available at http://agios.bitbucket.org/
 *
 * Contributors:
 *		Federal University of Rio Grande do Sul (UFRGS)
 *		INRIA France
 *
 *		This program is distributed in the hope that it will be useful,
 * 		but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _ESTIMATE_ACCESS_TIMES_H_
#define _ESTIMATE_ACCESS_TIMES_H_

void read_access_times_functions(char *filename);
unsigned long long int get_access_time(unsigned long int size, int operation);
float get_access_ratio(unsigned long int size, int operation);
#endif
