/* File:	estimate_access_times.c
 * Created: 	2015
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides an interface to obtain measured read/write processing
 *		time for different request sizes. It also provides the sequential
 *		to random throughput ratio. These values are provided to AGIOS 
 *		through an input file generated with SeRRa.
 *		Further information is available at http://agios.bitbucket.org/
 *
 * Contributors:
 *		Federal University of Rio Grande do Sul (UFRGS)
 *		INRIA France
 *
 *		This program is distributed in the hope that it will be useful,
 * 		but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdlib.h>
#include <stdio.h>
#include "estimate_access_times.h"
#include "agios.h"

static int intervals=0;
static unsigned long int **interval_sizes;
static double ***funcs;

#define SEQUENTIAL_WRITE 0
#define RANDOM_WRITE 1
#define SEQUENTIAL_READ 2
#define RANDOM_READ 3

void error_access_times_file(char *filename, FILE *fd)
{
	fprintf(stderr, "Error! Could not read from access times file %s\n", filename);
	if(fd)
		fclose(fd);
	exit(-1);

}

void read_access_times_functions(char *filename)
{
	FILE *fd;
	int ret, i, j;
	char *buffer = (char *)malloc(sizeof(char)*100);

	//open the file
	fd= fopen(filename, "r");
	if(!fd)
	{
		error_access_times_file(filename,fd);
	}

	//the first line is the number of intervals used in the profiling
	ret = fscanf(fd, "%d\n", &intervals);
	if(ret != 1)
		error_access_times_file(filename,fd);

	//allocate enough space for the data structures
	interval_sizes = (unsigned long int **)malloc(sizeof(unsigned long int *)*intervals);
	funcs = (double ***)malloc(sizeof(double **)*intervals);
	for(i=0; i<intervals; i++)
	{
		interval_sizes[i] = (unsigned long int *)malloc(sizeof(unsigned long int)*2);
		funcs[i] = (double **)malloc(sizeof(double *)*4);
		for(j=0; j<4; j++)
			funcs[i][j] = (double *)malloc(sizeof(double)*2);
	}

	//then we have a number of interval information entries. 
	for(i=0; i<intervals; i++)
	{
		//the first line gives the interval
		ret = fscanf(fd, "%lu\t%lu\n", &interval_sizes[i][0], &interval_sizes[i][1]);
		if(ret != 2)
			error_access_times_file(filename, fd);
		//the next four give the values
		for(j=0; j<4; j++)
		{
			ret = fscanf(fd, "%lf\t%lf\n", &funcs[i][j][0], &funcs[i][j][1]);
			if(ret != 2)
				error_access_times_file(filename, fd);
		} 
	}
	free(buffer);
	fclose(fd);
}

unsigned long long int get_access_time(unsigned long int size, int operation)
{
	int inter=intervals-1;
	int i, index;
	float this_size = (float)size/1024; //the functions were designed by providing request size in KB

	//we have to find out to which interval this size belongs to
	for(i=0; i< intervals; i++)
	{
		if(size <= interval_sizes[i][1]*1024) //intervals are described in KB, we are comparing with bytes
		{
			inter = i;
			break;
		}
	}	
	//if we don't find the size (because it is larger than the last interval), we will use the last one (we have initialized inter as intervals-1 to ensure that)

	//calculate the function
	if(operation == RT_READ)
		index = SEQUENTIAL_READ;
	else
		index = SEQUENTIAL_WRITE;

//	debug("looking for access time to a request of %lu bytes (%f Kbytes), type %d. It belongs to the interval %d, function is %lfx + %lf, results in %llu ns\n", size, this_size, operation, inter, funcs[inter][index][0], funcs[inter][index][1], (unsigned long long int) ( (funcs[inter][index][0]*this_size) + funcs[inter][index][1]));

	//time = ax + b 
	return (unsigned long long int)(funcs[inter][index][0]*this_size) + funcs[inter][index][1];
}

float get_access_ratio(unsigned long int size, int operation)
{
	int inter=intervals-1;
	int i, index, index_random;
	double time_seq, time_random;
	float this_size = (float)size/1024;

	//we have to find out to which interval this size belongs to
	for(i=0; i< intervals; i++)
	{
		if(size <= interval_sizes[i][1]*1024) //intervals are described in KB
		{
			inter = i;
			break;
		}
	}	
	//if we don't find the size (because it is larger than the last interval), we will use the last one (we have initialized inter as intervals-1 to ensure that)

	//calculate the function
	if(operation == RT_READ)
	{
		index = SEQUENTIAL_READ;
		index_random = RANDOM_READ;
	}
	else
	{
		index = SEQUENTIAL_WRITE;
		index_random = RANDOM_WRITE;
	}

	//time = ax + b 
	time_seq = (funcs[inter][index][0]*this_size) + funcs[inter][index][1];
	time_random = (funcs[inter][index_random][0]*this_size) + funcs[inter][index_random][1];

	//debug("looking for access time ratio to a request of %lu bytes (%f Kbytes), type %d. It belongs to the interval %d, sequential function is %lfx + %lf (results in %lf ns), random function is function is %lfx + %lf ( results in %lf ns), sequential throughput is %f, random throughput is %f, ratio is %f\n", size, this_size, operation, inter, funcs[inter][index][0], funcs[inter][index][1], time_seq, funcs[inter][index_random][0], funcs[inter][index_random][1], time_random, (this_size / time_seq), (this_size /  time_random), (float) (this_size /  time_seq) / (this_size /  time_random)  );



	//calculate the ratio by dividing the sequential throughput by the random one
	return (float) (this_size / time_seq) / (this_size / time_random);
}