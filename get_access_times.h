/* File:	agios.c
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides an interface to obtain measured read/write processing
 *		time for different request sizes. It also provides the sequential
 *		to random throughput ratio. These values are provided to AGIOS 
 *		through the access_times.h and access_times_ratios.h files.
 *		Further information is available at http://inf.ufrgs.br/~fzboito/agios.html
 *
 * Contributors:
 *		Federal University of Rio Grande do Sul (UFRGS)
 *		INRIA France
 *
 *		This program is distributed in the hope that it will be useful,
 * 		but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _GET_ACCESS_TIMES_H_
#define _GET_ACCESS_TIMES_H_


unsigned long long int get_access_time(unsigned long int size, int operation);
float get_access_ratio(long long size, int operation);

#endif
