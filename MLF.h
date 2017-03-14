#ifndef _MLF_H_
#define _MLF_H_

#define MAX_MLF_LOCK_TRIES	2
#define MAX_AGGREG_SIZE_MLF   200

int MLF_init();
void MLF_exit();
void MLF(void *clnt);
#endif
