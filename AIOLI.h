#ifndef _AIOLI_H_
#define _AIOLI_H_

#define MAX_AGGREG_SIZE   16
#define ANTICIPATORY_VALUE(qt,op) (2*get_access_time(qt,op)) /*the initial quantum given to the requests (usually twice the necessary time to process a request of size MLF_QUANTUM)*/

int AIOLI_init();
void AIOLI(void *clnt);
#endif
