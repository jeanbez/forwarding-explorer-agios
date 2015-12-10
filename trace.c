/* File:	trace.c
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides the Trace Module, that generates trace files.
 *		Trace generation, trace files' name and verbosity are defined
 *		through parameters in the configuration file.
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
#ifndef AGIOS_KERNEL_MODULE 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trace.h"
#include "common_functions.h"
#include "predict.h"
#include "agios_config.h"

/*the current trace file*/
static FILE *agios_tracefile_fd;
static int agios_trace_counter=-1; //the trace files are numbered, so during initialization we figure out how many there are and create the next one. Trace files' names are given with the numbers between prefix and sufix defined at the configuration file

static pthread_mutex_t agios_trace_mutex = PTHREAD_MUTEX_INITIALIZER; //since multiple threads call the Trace Module's functions, this mutex makes sure only one tries to access the trace file at a time. The thread calling the functions MUST NOT lock it, the function itself handles it.
static struct timespec agios_trace_t0; //time measured at initialization (all traced times are relative to this one)

//a buffer avoids generating many I/O operations to the trace file, which is stored in the local file system. This would affect the scheduler's results, since these operations are not schedule with the user's ones.
static char *agios_tracefile_buffer=NULL;
static int agios_tracefile_buffer_size=0; //occupancy of the buffer. Used to control when to flush it.
static char *aux_buf = NULL; //this smaller buffer is used by the functions to write a line at a time to the main buffer. We keep it global to avoid having to allocate it multiple times (it is allocated during initialization)
static int aux_buf_size = 300*sizeof(char); //hard coded

/*configuration options. Obtained at initialization for performance reasons*/
static short int agios_config_trace_full=0;
static long int agios_config_max_trace_buffer_size = 1*1024*1024;

//must have trace lock
void agios_tracefile_flush_buffer()
{
	/*write it*/
	if(fprintf(agios_tracefile_fd, "%s", agios_tracefile_buffer) < agios_tracefile_buffer_size)
	{
		agios_print("PANIC! Could not write trace buffer to trace file!\n");
	}
	fflush(agios_tracefile_fd);
	/*reset the bufer*/
	agios_tracefile_buffer_size=0;
}
//must have trace lock
void agios_trace_write_to_buffer()
{
	int size = strlen(aux_buf);
	if((agios_tracefile_buffer_size + size) >= agios_config_max_trace_buffer_size)
	{
		agios_tracefile_flush_buffer();
	}
	snprintf(agios_tracefile_buffer+agios_tracefile_buffer_size, size+1, "%s", aux_buf);
	agios_tracefile_buffer_size += size;
	aux_buf[0]='\0';
}
//a shift phenomenon was detected and the scheduler decided to wait for a file (for aIOLi and MLF only)
void agios_trace_shift(unsigned long long int wait_time, char *file)
{
	if(!agios_config_trace_full)
		return;

	agios_mutex_lock(&agios_trace_mutex);

	snprintf(aux_buf, aux_buf_size, "[SHIFT PHENOMENON]\t%llu\t%s\t%llu\n", get_nanoelapsed(agios_trace_t0), file, wait_time);
	agios_trace_write_to_buffer();
	
	agios_mutex_unlock(&agios_trace_mutex);

}
//schedulerr is going to wait for a file
void agios_trace_wait(unsigned long long int wait_time, char *file)
{
	if(!agios_config_trace_full)
		return;

	agios_mutex_lock(&agios_trace_mutex);
	
	snprintf(aux_buf, aux_buf_size, "[AGIOS WAIT]\t%llu\t%s\t%llu\n", get_nanoelapsed(agios_trace_t0), file, wait_time);
	agios_trace_write_to_buffer();

	agios_mutex_unlock(&agios_trace_mutex);
}
//it was detected that a better aggregation to a file was already performed, so the scheduler decided to wait 
void agios_trace_better(char *file)
{
	if(!agios_config_trace_full)
		return;

	agios_mutex_lock(&agios_trace_mutex);
	
	snprintf(aux_buf, aux_buf_size, "[BETTER AGGREGATION]\t%llu\t%s\t%d\n", get_nanoelapsed(agios_trace_t0), file, config_waiting_time);
	agios_trace_write_to_buffer();

	agios_mutex_unlock(&agios_trace_mutex);
}
//a better aggregation was predicted, so the scheduler decided to wait
void agios_trace_predicted_better_aggregation(unsigned long long int wait_time, char *file)
{
	if(!agios_config_trace_full)
		return;

	agios_mutex_lock(&agios_trace_mutex);
	
	snprintf(aux_buf, aux_buf_size, "[PREDICTED BETTER AGGREGATION]\t%llu\t%s\t%llu\n", get_nanoelapsed(agios_trace_t0), file, wait_time);
	agios_trace_write_to_buffer();

	agios_mutex_unlock(&agios_trace_mutex);
}
//must have trace lock
void agios_trace_print_request(struct request_t *req)
{
	int index = strlen(aux_buf);
	if(req->type == RT_READ)
		snprintf(aux_buf+index, aux_buf_size - index, "%s\tR\t%lld\t%ld\n", req->file_id, req->io_data.offset, req->io_data.len);
	else
		snprintf(aux_buf+index, aux_buf_size - index, "%s\tW\t%lld\t%ld\n", req->file_id, req->io_data.offset, req->io_data.len);	
}
//must have trace lock
void agios_trace_print_predicted_request(struct request_t *req)
{
	int size = strlen(aux_buf);
	if(req->type == RT_READ)
		snprintf(aux_buf+size, aux_buf_size - size, "%s\tR\t%lld\t%ld\t%llu\n", req->file_id, req->io_data.offset, req->io_data.len, req->jiffies_64);
	else
		snprintf(aux_buf+size, aux_buf_size - size, "%s\tW\t%lld\t%ld\t%llu\n", req->file_id, req->io_data.offset, req->io_data.len, req->jiffies_64);
}
//a new request arrived to the scheduler
void agios_trace_add_request(struct request_t *req)
{
	agios_mutex_lock(&agios_trace_mutex);
	snprintf(aux_buf, aux_buf_size, "%llu\t", (req->jiffies_64 - get_timespec2llu(agios_trace_t0)));
	agios_trace_print_request(req);
	agios_trace_write_to_buffer();
	
	agios_mutex_unlock(&agios_trace_mutex);
}

//a predicted request was added to the queues
void agios_trace_predict_addreq(struct request_t *req)
{
	if(!config_get_trace_predict())
		return;

	agios_mutex_lock(&agios_trace_mutex);

	sprintf(aux_buf, "[PREDICTED REQUEST]\t");
	agios_trace_print_predicted_request(req);
	agios_trace_write_to_buffer();
	
	agios_mutex_unlock(&agios_trace_mutex);
}
//a virtual request was processed
void agios_trace_process_requests(struct request_t *head_req)
{
	struct request_t *req;
	int size;

	if(!agios_config_trace_full)
		return;

	agios_mutex_lock(&agios_trace_mutex);

	snprintf(aux_buf, aux_buf_size, "[VIRTUAL REQUEST PROCESSED]\t%llu\t%s\n",get_nanoelapsed(agios_trace_t0) , head_req->file_id);
	agios_trace_write_to_buffer();
	if(head_req->reqnb == 1)
	{
		agios_trace_print_request(head_req);
	}
	else
	{
		agios_list_for_each_entry(req, &head_req->reqs_list, aggregation_element)
		{
			agios_trace_print_request(req);
			agios_trace_write_to_buffer();
		}
	}
	size = strlen(aux_buf);
	snprintf(aux_buf + size, aux_buf_size - size, "[END OF VIRTUAL REQUEST]\n");
	agios_trace_write_to_buffer();

	agios_mutex_unlock(&agios_trace_mutex);	
}
//must have trace lock
void trace_print_predicted_aggregations_onlist(struct related_list_t *predicted_l)
{
	struct request_t *req, *head;
	struct agios_list_head *req_l = NULL;

	req_l = predicted_l->list.next;
	while((req_l) && (req_l != &(predicted_l->list)))
	{
		req = agios_list_entry(req_l, struct request_t, related);
		if(!req)
			break;
		if(req->predicted_aggregation_size > 1)
		{
			/*we found the start of an aggregation*/
			head = req;
			agios_trace_print_predicted_request(req);
			agios_trace_write_to_buffer();

			req_l = req_l->next;
			req = agios_list_entry(req_l, struct request_t, related);
			while((req_l) && (req) && (req_l != &(predicted_l->list)) && (req->predicted_aggregation_start == head))
			{
				agios_trace_print_predicted_request(req);
				agios_trace_write_to_buffer();
				req_l = req_l->next;
				req = agios_list_entry(req_l, struct request_t, related);
			}
			snprintf(aux_buf, aux_buf_size, "---\n");
			continue;
		}
		req_l = req_l->next;
	}
}
//after the Prediction Module predicted all aggregations, prints them
void agios_trace_print_predicted_aggregations(struct request_file_t *req_file)
{
	int size;

	if(!config_get_trace_predict())
		return;

	agios_mutex_lock(&agios_trace_mutex);

	snprintf(aux_buf, aux_buf_size, "[PREDICTED AGGREGATIONS]\t%s\t%llu\n", req_file->file_id, get_nanoelapsed(agios_trace_t0) );

	trace_print_predicted_aggregations_onlist(&req_file->predicted_reads);
	trace_print_predicted_aggregations_onlist(&req_file->predicted_writes);

	size = strlen(aux_buf);
	snprintf(aux_buf + size, aux_buf_size - size, "[END OF PREDICTED AGGREGATIONS]\n");
	agios_trace_write_to_buffer();

	agios_mutex_unlock(&agios_trace_mutex);
}


int agios_trace_init()
{
	char filename[256];	
	int ret;

	agios_mutex_lock(&agios_trace_mutex);
	
	agios_gettime(&agios_trace_t0);
		

	if(agios_trace_counter == -1)
	{
		/*we have to find out how many trace files are there*/
		agios_trace_counter=0;
		do {
			if(agios_trace_counter > 0)
				fclose(agios_tracefile_fd);

			agios_trace_counter++;
			sprintf(filename, "%s.%d.%s", config_get_trace_file_name(1), agios_trace_counter, config_get_trace_file_name(2));	
			agios_tracefile_fd = fopen(filename, "r");
		} while(agios_tracefile_fd);
	}
	else
	{
		agios_trace_counter++;
		sprintf(filename, "%s.%d.%s", config_get_trace_file_name(1), agios_trace_counter, config_get_trace_file_name(2));
	}
		
	/*create and open the new trace file*/
	agios_tracefile_fd = fopen(filename,  "w+");

	if(agios_tracefile_fd)
	{
		/*prepare the buffer*/
		if(agios_tracefile_buffer)
			agios_tracefile_buffer_size=0; //we already have a buffer, just have to reset it
		else
		{
			agios_tracefile_buffer = (char *)malloc(agios_config_max_trace_buffer_size); 
		}
		ret =  agios_trace_counter;
	}
	else
		ret =  -1;

	agios_config_max_trace_buffer_size = config_get_max_trace_buffer_size();
	agios_config_trace_full = config_get_trace_full(); //TODO if we were to change the configuration options during execution, we would need to update this. It is like this for performance reasons

	if(!aux_buf)
	{
		aux_buf = (char *)malloc(aux_buf_size); //TODO could we have a trace line with more than 300 characters? It would depend on filenames...
		if(!aux_buf)
		{
			agios_print("PANIC! Could not allocate memory for trace file buffer!\n");
			ret = -1;
		}
	}

	agios_mutex_unlock(&agios_trace_mutex);

	return ret;
}
void agios_trace_close()
{
	agios_mutex_lock(&agios_trace_mutex);
	agios_tracefile_flush_buffer();
	fclose(agios_tracefile_fd);
	agios_mutex_unlock(&agios_trace_mutex);
}
void agios_trace_reset()
{
	agios_trace_close();
	prediction_notify_new_trace_file();
	agios_trace_init();
}
#endif //ifndef AGIOS_KERNEL_MODULE
