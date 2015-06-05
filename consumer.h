/* File:	consumer.h
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the code for the AGIOS thread. It sleeps when there are no 
 *		requests, and calls the scheduler otherwise.
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
#ifndef _CONSUMER_H_
#define _CONSUMER_H_

#ifdef AGIOS_KERNEL_MODULE
int agios_thread(void *arg);
#else
void * agios_thread(void *arg);
#endif


void stop_the_agios_thread(void);
void process_requests(struct request_t *head_req, struct client *clnt, int hash);

#endif
