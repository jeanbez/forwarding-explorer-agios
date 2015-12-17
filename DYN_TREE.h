/* File:	DYN_TREE.h
 * Created: 	2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the dynamic scheduling algorithm selection through selection trees 
 *		(the algorithm used in Francieli's thesis)
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

#ifndef _DYN_TREE_H_
#define _DYN_TREE_H_

int DYN_TREE_init(void);
int DYN_TREE_select_next_algorithm(void);
#endif
