/* File:	agios.c
 * Created: 	2012 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides an interface to obtain measured read/write processing
 *		time for different request sizes. It also provides the sequential
 *		to random throughput ratio. These values are provided to AGIOS 
 *		through the access_times.h and access_times_ratios.h files.
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
#include "get_access_times.h"
#include "agios.h"
#include "access_times.h"
#include "access_times_ratios.h"


unsigned long long int get_average(int index, unsigned long int size, int operation)
{
	if(writing_times[index][0] > size)
	{
		if(operation == RT_WRITE)
			return (writing_times[index-1][1] + writing_times[index][1])/2;
		else
			return (reading_times[index-1][1] + reading_times[index][1])/2;
	}
	else
	{
		if(operation == RT_WRITE)
			return (writing_times[index+1][1] + writing_times[index][1])/2;
		else
			return (reading_times[index+1][1] + reading_times[index][1])/2;
	
	}
}

/*obtain the measured access time from the table in access_times.h. If not measured for this size, takes the average between the two closest neighbours*/
unsigned long long int get_access_time(unsigned long int size, int operation)
{
	int index;

	if(size < writing_times[0][0])
	{
		if(operation == RT_WRITE)
			return (writing_times[0][1]/writing_times[0][0])*size;
		else
			return (reading_times[0][1]/reading_times[0][0])*size;
	}
	else if(size > MAX_BENCHMARK_SIZE*1024)
	{
		if(operation == RT_WRITE)
			return (writing_times[NUMBER_OF_TESTS-1][1]*size)/writing_times[NUMBER_OF_TESTS-1][0];
		else
			return (reading_times[NUMBER_OF_TESTS-1][1]*size)/reading_times[NUMBER_OF_TESTS-1][0];

	}
	else
	{
		index = ((size/1024)/BENCHMARK_STEP);
		if(writing_times[index][0] != size)
			return get_average(index, size, operation);
		else if(operation == RT_WRITE)
			return writing_times[index][1];
		else
			return reading_times[index][1];
	}	
}
/*get the sequential to random throughput ratio for this operation and this request size from the table in access_times_ratio.h.*/
float get_access_ratio(long long size, int operation)
{
	int index;

	//size is either too small or the first value, we will use the ratio for the first measured request size
	if(size <= writing_ratios[0][0])
	{
		if(operation == RT_WRITE)
			return writing_ratios[0][1];
		else
			return reading_ratios[0][1];
	}
	else if(size >= writing_ratios[NUMBER_OF_RATIOS-1][0]) //size is either too big or the last value, we will use the ratio for the last measured request size
	{
		if(operation == RT_WRITE)
			return writing_ratios[NUMBER_OF_RATIOS-1][1];
		else
			return reading_ratios[NUMBER_OF_RATIOS-1][1];
	}
	else //size is between the measured values
	{
		index = ((size/1024)/RATIOS_STEP)-1; //we are assuming the size will be one of the measured (or we will have to accept a close ratio, measured for another request size)
		if(operation == RT_WRITE)
			return writing_ratios[index][1];
		else
			return reading_ratios[index][1];
	}
}
