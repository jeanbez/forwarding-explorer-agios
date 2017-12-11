
#ifndef _AGIOS_COMMON_FUNCTIONS_H_
#define  _AGIOS_COMMON_FUNCTIONS_H_

#include "agios.h"

long int agios_min(long int t1, long int t2);
long int agios_max(long int t1, long int t2);
short int get_index_max(int *count);


/*functions that are different for the kernel module and the user-level library versions*/
#ifdef AGIOS_KERNEL_MODULE

#include <linux/time.h>
#include <linux/mutex.h>

typedef int (thread_function) (void *arg); 

int agios_start_thread_km(struct task_struct *thread, thread_function *func, char *name, void *arg);

#define agios_alloc(size) 					kmalloc(size, GFP_KERNEL)
#define agios_free(ptr) 					kfree(ptr)

#define agios_gettime(timev)					do_posix_clock_monotonic_gettime(timev)

#define agios_mutex_destroy(mutex)				(void)(0)
#define agios_mutex_init(mutex)					mutex_init(mutex)
#define agios_mutex_lock(mutex) 				mutex_lock(mutex)
#define agios_mutex_unlock(mutex) 				mutex_unlock(mutex)
#define SIZE_OF_AGIOS_MUTEX_T 					sizeof(struct mutex)

#define agios_print(f, a...) 					printk(KERN_ALERT "AGIOS: " f "\n", ## a) 

#define agios_start_thread(thread, function, name, arg)		agios_start_thread_km(thread, function, name, arg)

#else /*NOT the kernel module version*/

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define agios_alloc(size) 					malloc(size)
#define agios_free(ptr)						free(ptr)

#define agios_gettime(timev)					clock_gettime(CLOCK_MONOTONIC, timev)

#define agios_cond_init(cond)					pthread_cond_init(cond, NULL)
#define agios_cond_signal(cond)					pthread_cond_signal(cond)
#define agios_cond_broadcast(cond)				pthread_cond_broadcast(cond)
#define agios_cond_wait(cond, mutex)				pthread_cond_wait(cond, mutex)
#define agios_mutex_destroy(mutex)				pthread_mutex_destroy(mutex);
#define agios_mutex_init(mutex)					pthread_mutex_init(mutex, NULL)
#define agios_mutex_lock(mutex) 				pthread_mutex_lock(mutex)
#define agios_mutex_unlock(mutex) 				pthread_mutex_unlock(mutex)
#define SIZE_OF_AGIOS_MUTEX_T 					sizeof(pthread_mutex_t)

#define agios_print(f, a...) 					fprintf(stderr, "AGIOS: " f "\n", ## a)
#define agios_just_print(f, a...) 				fprintf(stderr, f, ## a)

#define agios_start_thread(thread, function, name, arg)		pthread_create(&thread, NULL, function, arg) 

#endif /*end ifdef AGIOS_KERNEL_MODULE - else*/

/****************************************************************************************************/
/*timing functions*/
/*call with (struct timespec t1, struct timespec t2) */
#define get_nanoelapsed_diff(t1, t2)  ((t2.tv_nsec - t1.tv_nsec) + ((t2.tv_sec - t1.tv_sec)*1000000000L))
/*takes a struct timespec and tells you how many nanoseconds passed since the timespec was obtained*/
long int get_nanoelapsed(struct timespec t1);
/*translates a struct timespec to a long int (in nanoseconds)*/
long int get_timespec2long(struct timespec t);
/*traslates a long int (in nanoseconds) to struct timespec*/
void get_long2timespec(long int t, struct timespec *ret);
/*does the same as get_nanoelapsed, but takes as parameter a long int instead of a struct timespec*/
long int get_nanoelapsed_long(long int t1);
double get_ns2s(long int t1);

/****************************************************************************************************/
/*debug functions*/
#ifdef AGIOS_DEBUG

#define PRINT_FUNCTION_NAME agios_print("%s\n", __PRETTY_FUNCTION__)
#define PRINT_FUNCTION_EXIT agios_print("%s exited\n", __PRETTY_FUNCTION__)
#define debug(f, a...) agios_print("%s(): " f "\n", __PRETTY_FUNCTION__, ## a)
#ifndef AGIOS_KERNEL_MODULE
#define agios_panic(f, a...)	\
	agios_print("%s: " f "\n", __PRETTY_FUNCTION__, ## a); exit(-1)
#else
#define agios_panic(f, a...) debug(f, ## a)
#endif
#define VERIFY_REQUEST(req) 						\
	if (req->sanity != 123456) {					\
		agios_panic("It isn't a request!!!");			\
	}

#else //NOT in debug mode

#define PRINT_FUNCTION_NAME (void)(0)
#define PRINT_FUNCTION_EXIT (void)(0)
#define agios_panic(f, a...) (void)(0)
#define debug(f, a...) (void)(0)
#define VERIFY_REQUEST(req) (void)(0)

#endif /*end ifdef AGIOS_DEBUG - else*/

#ifdef AGIOS_KERNEL_MODULE
#define O_CREAT 0
#define O_TRUNC 0
#endif

#endif // _AGIOS_COMMON_FUNCTIONS_H_

