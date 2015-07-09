/* File:	agios.h
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the interface to its users
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

/*
 * Request type.
 */
enum {
	RT_READ = 0,
	RT_WRITE = 1,
};

struct client {

	/*
	 * Callback functions
	 */
#ifdef ORANGEFS_AGIOS
	void (*process_request)(int64_t req_id);
	void (*process_requests)(int64_t *reqs, int reqnb);
#else
	void (*process_request)(int i);
	void (*process_requests)(int *i, int reqnb);
#endif

	short int sync; //this parameter is decided by AGIOS, after choosing the scheduling algorithm. 

	/*
	 * aIOLi calls this to check if device holding file is idle.
	 * Should be supported by FS client
	 */
	int (*is_dev_idle)(void);   //never used
};

int agios_init(struct client *clnt, char *config_file);
void agios_exit(void);


#ifdef ORANGEFS_AGIOS
int agios_add_request(char *file_id, int type, long long offset,
		       long len, int64_t data, struct client *clnt);
#else
int agios_add_request(char *file_id, int type, long long offset,
		       long len, int data, struct client *clnt);
#endif


void agios_print_stats_file(char *filename);
void agios_reset_stats(void);

void agios_wait_predict_init(void);

