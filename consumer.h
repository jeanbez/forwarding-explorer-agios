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
void consumer_init(struct client *clnt_value, struct task_struct *task_value);
void consumer_set_task(struct task_struct *value);
struct task_struct * consumer_get_task(void);
#else
void * agios_thread(void *arg);
void consumer_init(struct client *clnt_value, int task_value);
void consumer_set_task(int value);
int consumer_get_task(void);
#endif

struct io_scheduler_instance_t *consumer_get_current_scheduler(void);

void consumer_signal_new_reqs(void);

void stop_the_agios_thread(void);
short int process_requests(struct request_t *head_req, struct client *clnt, int hash); //returns a flag pointing if some refresh period has expired (scheduling algorithm should exit so we can check it and perform the necessary actions)

#endif
