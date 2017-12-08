/* File:	common_functions.c
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It defines some functions widely used by all other files, especially the ones
 *		that are different between kernel module and user-level library implementations, 
 *		besides timing functions and debug interface
 *		Further information is available at http://inf.ufrgs.br/~fzboito/agios.html
 *
 * Contributors:
 *		Federal University of Rio Grande do Sul (UFRGS)
 *		INRIA France
 *
 *		This program is distributed in the hope that it will be useful,
 * 		but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */
#include "common_functions.h"



long int agios_min( long int t1,  long int t2)
{
	if(t1 < t2)
		return t1;
	else
		return t2;
}
long int agios_max( long int t1, long int t2)
{
	if(t1 > t2)
		return t1;
	else
		return t2;
}

#ifdef AGIOS_KERNEL_MODULE

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include <linux/file.h>
#include <linux/fs.h>



int agios_start_thread_km(struct task_struct *thread, thread_function *func, char *name, void *arg)
{
	thread = kthread_run(func, arg, name);
	/*TODO test to see if the function worked*/
	return 0;

}

#endif /*end ifdef AGIOS_KERNEL_MODULE - else*/

/****************************************************************************************************/
/*timing functions*/
/*takes a struct timespec and tells you how many nanoseconds passed since the timespec was obtained*/
long int get_nanoelapsed(struct timespec t1)
{
	struct timespec t2;
	agios_gettime(&t2);
	return ((t2.tv_nsec - t1.tv_nsec) + ((t2.tv_sec - t1.tv_sec)*1000000000L));
}

/*translates a struct timespec to a long int (in nanoseconds)*/
long int get_timespec2long(struct timespec t)
{
	return (t.tv_sec*1000000000L + t.tv_nsec);
}

/*traslates a long int (in nanoseconds) to struct timespec*/
void get_long2timespec(long int t, struct timespec *ret)
{
	ret->tv_sec = t / 1000000000L;
	ret->tv_nsec = t % 1000000000L;
}

/*does the same as get_nanoelapsed, but takes as parameter a long int instead of a struct timespec*/
long int get_nanoelapsed_long(long int t1)
{
	struct timespec t2;
	agios_gettime(&t2);
	return (get_timespec2long(t2) - t1);
}
double get_ns2s(long int t1)
{
	double ret = ((double)t1) / 1000.0;
	ret = ret / 1000.0;
	return ret / 1000.0;
}

