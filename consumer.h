#ifndef _CONSUMER_H_
#define _CONSUMER_H_

#ifdef AGIOS_KERNEL_MODULE
extern struct task_struct	*consumer_task;
int agios_thread(void *arg);
void consumer_init(struct client *clnt_value, struct task_struct *task_value);
struct task_struct * consumer_get_task(void);
#else
extern int consumer_task; 
void * agios_thread(void *arg);
void consumer_init(struct client *clnt_value, int task_value);
int consumer_get_task(void);
#endif

void consumer_signal_new_reqs(void);

void stop_the_agios_thread(void);
short int process_requests(struct request_t *head_req, struct client *clnt, int hash); //returns a flag pointing if some refresh period has expired (scheduling algorithm should exit so we can check it and perform the necessary actions)

#ifndef AGIOS_KERNEL_MODULE
extern int agios_thread_stop;
extern pthread_mutex_t agios_thread_stop_mutex;
#endif

#endif
