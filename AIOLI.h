/* File:	AIOLI.h
 * Created: 	2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 * Collaborators:
 *		Jean Luca Bez <jlbez (at) inf.ufrgs.br>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the aIOLi scheduling algorithm
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

#ifndef _AIOLI_H_
#define _AIOLI_H_

#define MAX_AGGREG_SIZE   16
#define ANTICIPATORY_VALUE(qt,op) (2*get_access_time(qt,op)) /*the initial quantum given to the requests (usually twice the necessary time to process a request of size MLF_QUANTUM)*/

int AIOLI_init();
void AIOLI(void *clnt);
#endif
