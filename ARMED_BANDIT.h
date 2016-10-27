/* File:	ARMED_BANDIT.h
 * Created: 	2015 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 * Collaborators:
 *		Ramon Nou < ramon.nou (at) bsc.es>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the Armed Bandit approach implementation
 *		Further information is available at http://agios.bitbucket.org/
 *
 * Contributors:
 *		Federal University of Rio Grande do Sul (UFRGS)
 *		INRIA France
 *		Barcelona Supercomputing Center
 *
 *		This program is distributed in the hope that it will be useful,
 * 		but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef _ARMED_BANDIT_H_
#define _ARMED_BANDIT_H_

int ARMED_BANDIT_init(void);
int ARMED_BANDIT_select_next_algorithm(void);
void ARMED_BANDIT_exit(void);
void write_migration_end(unsigned long long int timestamp);
int ARMED_BANDIT_aux_init(void);
inline void ARMED_BANDIT_set_current_sched(int new_sched);
unsigned long long int ARMED_BANDIT_update_bandwidth(double *recent_measurements, short int cleanup);
#endif
