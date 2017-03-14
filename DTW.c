/*
 * Copyright (c) 2004 Stan Salvador
 * stansalvador@hotmail.com

 The MIT License (MIT)

Copyright (c) 2004 Stan Salvador

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

 *  Translated from JAVA to C++ by Ramon Nou - Barcelona Supercomputing Center - IOLanes EU Project
 *  Translated to C and adapted and optimized by Francieli Zanon Boito 
 */

/* File:	DTW.c
 * Created: 	November 2016 
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It contains the fastDTW method implementation to compare access patterns	
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

/* TO UNDERSTAND THIS PART: Go and look to the last function, which is the one called from outside this module, and then see the path to the other function calls */

#include <math.h>
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "common_functions.h"
#include "DTW.h"



struct time_warp_info_t * Check_TimeWarpInfo_allocation(int tsI_reqnb, int tsJ_reqnb);
long long int ** Check_cMatrix_allocation(int tsI_reqnb, int tsJ_reqnb);
void Add_to_TimeWarp_Path(struct time_warp_info_t *info, int a, int b);
struct search_window_iterator_t *SearchWindow_obtain(struct search_window_t *window);
//=====================================================================================================================================
//helper functions, to get max and min among values
long long int min(long long int value1, long long int value2)
{
	long long int ret;
	if(value1 > value2)
		ret = value2;
	else
		ret = value1;
	return ret;
}
long long int max(long long int value1, long long int value2)
{
	if(value1 >= value2)
		return value1;
	else
		return value2;
}
long long int positive_sum(long long int value1, long long int value2)
{
	long long int ret = value1+value2;
	if(ret < value1)
		ret = LLONG_MAX;
	return ret;
}
//calculates the distance between two elements of the time series. This element may have multiple dimensions
long long int DTW_euclideanDist(struct access_pattern_t *tsI, struct access_pattern_t *tsJ, int i, int j)
{
	//TODO make it ready for multiple dimensions!	
//	return pow(sqrt(pow(tsI->time_series[indexI].offset - tsJ->time_series[indexJ].offset, 2.0)), 2.0);
//actually this was to compare vectors, since we are using 1d dtw, we will only compare pairs of values, so:
	if(tsI->time_series[i].offset >= tsJ->time_series[j].offset)
		return (tsI->time_series[i].offset - tsJ->time_series[j].offset);
	else
		return (tsJ->time_series[j].offset - tsI->time_series[i].offset);
}
//function used when shrinking an access pattern to a smaller one, used to combine several points into a single one
void combine_access_pattern_points(struct pattern_tracker_req_info_t *point, struct access_pattern_t *ts, int ptToReadFrom, int ptToReadTo)
{
	long long int sum_offsets;
	int pt;
	//TODO make it ready for multiple dimensions!

	//keep track of the sum of all the values being averaged to create a single point
	sum_offsets = 0;
	//sum all values over the range
	for(pt = ptToReadFrom; pt <= ptToReadTo; pt++)
	{
		sum_offsets += ts->time_series[pt].offset;
	}
	//could we have exceeded what a long long int can keep?
	if(sum_offsets < 0)
		sum_offsets = LLONG_MAX;
	//determine the average of the range and add to the new time series
	point->offset = sum_offsets/(ptToReadTo-ptToReadFrom+1);
}
//=====================================================================================================================================
//full DTW, called by FastDTW at the end of the recursion, for the smallest versions of the access patterns (or we can use it directly)
struct time_warp_info_t * DTW_DynamicTimeWarp(struct access_pattern_t *tsI, struct access_pattern_t *tsJ, short int distanceonly)
{
	int i,j;
//	PRINT_FUNCTION_NAME;
	struct time_warp_info_t *ret = Check_TimeWarpInfo_allocation(tsI->reqnb, tsJ->reqnb); 
	if(ret == NULL)
	{
		agios_print("PANIC! Could not allocate memory for DTW");
		return NULL;
	}

	//allocate a cost matrix
	long long int **costMatrix = Check_cMatrix_allocation(tsI->reqnb, tsJ->reqnb);
	if(!costMatrix)
	{
		agios_print("PANIC! Could not allocate cost matrix for DTW");
		return NULL;
	}
	//initialize the cost matrix with 0 in all positions
	for(i = 0; i < tsI->reqnb; i++)
	{
		for(j=0; j < tsJ->reqnb; j++)
			costMatrix[i][j] = 0;
	}


	//calculate the values for the first column
	costMatrix[0][0] = DTW_euclideanDist(tsI, tsJ, 0, 0);
	for(j = 1; j < tsJ->reqnb; j++)
	{
		costMatrix[0][j] = positive_sum(costMatrix[0][j-1],DTW_euclideanDist(tsI, tsJ, 0, j));
	}
	//now for the rest of the matrix
	for(i = 1; i< tsI->reqnb; i++) 
	{
		//calculate for the bottom row
		costMatrix[i][0] = positive_sum(costMatrix[i-1][0],DTW_euclideanDist(tsI, tsJ, i, 0));
		//now for the other rows
		for(j = 1; j < tsJ->reqnb; j++)
			costMatrix[i][j] = positive_sum((min(costMatrix[i-1][j], min(costMatrix[i-1][j-1], costMatrix[i][j-1]))), DTW_euclideanDist(tsI, tsJ, i, j));
	}
	//ok, now we have minimum cost
	ret->distance = costMatrix[tsI->reqnb-1][tsJ->reqnb-1];

//	debug("found distance %lld\n", ret->distance);

	if(!distanceonly) //we might not need to generate the path (if we are using DTW, not fast dtw)
	{
		//find the warp path by searching the matrix form the solution to at (maxI, maxJ) to the beginning at (0,0). At each step move through the matrix 1 step left, down, or diagonal, whichever has the smallest cost. Favor diagonal moves and moves towards the i==j axis to break ties.
		i = tsI->reqnb-1;
		j = tsJ->reqnb-1;
		Add_to_TimeWarp_Path(ret, i, j);
		while((i > 0) || (j > 0))
		{
			//find the costs of moving in all three possible directions: left, down, and diagonal (down and left at the same time)
			long long int diagCost, leftCost, downCost;
			
			if(( i > 0) & (j > 0))
				diagCost = costMatrix[i-1][j-1];
			else
				diagCost = LLONG_MAX;
			if (i > 0)
				leftCost = costMatrix[i-1][j];
			else
				leftCost = LLONG_MAX;
			if(j > 0)
				downCost = costMatrix[i][j-1];
			else
				downCost = LLONG_MAX;
			//determine which direction to move in. Prefer moving diagonally and moving towards i == j axis of the matrixif there are ties
			if((diagCost <= leftCost) && (diagCost <= downCost) && (i > 0) && (j > 0))
			{
				i--;
				j--;
			}
			else if ((leftCost <diagCost) && (leftCost < downCost) && (i > 0))
				i--;
			else if ((downCost < diagCost) && (downCost < leftCost) && (j > 0))
				j--;
			else if ((i <= j) && (j > 0)) //leftCost == rightCost > diagCost
				j--;
			else if (i > 0)
				i--;
			else
				j--;
			//add the current step to the warp path
//			debug("adding %d %d\n", i, j);
			Add_to_TimeWarp_Path(ret, i, j);
		}
	}

	
//	PRINT_FUNCTION_EXIT;
	return ret;
}
//function to call directly, applies full DTW and returns the distance only (without the path)
long long int DTW(struct access_pattern_t *tsI, struct access_pattern_t *tsJ)
{
	struct time_warp_info_t *info = DTW_DynamicTimeWarp(tsI, tsJ, 1); //we ask DTW to calculate the distance only, no need to recreate the whole path if we are not using it
	if(info)
		return info->distance;
	else
	{
		agios_print("PANIC! Could not apply fast DTW");
		return LLONG_MAX;
	}
}
//=====================================================================================================================================
//functions to manage - allocate, access, free - a constrainedDTW_cost_t structure, used by constrained DTW.
//this structure has two arrays, one of long long ints, called cellValues, and another of ints, called colOffsets. Each one has a _size int to keep track of values.
//we'll try to keep this structure allocated between executions, so we won't have to re-allocate and free it every time we call constrainedDTW 
int max_constrainedDTW_cost_columns=0;
int max_constrainedDTW_cost_points=0;
struct constrainedDTW_cost_t *CDTW_cost=NULL;

struct constrainedDTW_cost_t *Check_constrainedDTWcost_allocation(struct search_window_t *window)
{
	int i;
	int currentOffset=0;

	//if the structure is not allocated, we'll start by doing that
	if(!CDTW_cost)
	{
		CDTW_cost = malloc(sizeof(struct constrainedDTW_cost_t));
		if(!CDTW_cost)
		{
			agios_print("PANIC! Could not allocate memory for constrained DTW cost matrix");
			max_constrainedDTW_cost_columns = max_constrainedDTW_cost_points =0;
			return NULL;
		}
		CDTW_cost->cellValues = NULL;
		CDTW_cost->colOffsets = NULL;
	}
	//colOffsets will have length window->maxJ. To each j, it will have the number of points in the window in all columns smaller than j
	if(max_constrainedDTW_cost_columns < window->maxJ)
	{
		//we'll free it and then allocate again
		if(CDTW_cost->colOffsets)
			free(CDTW_cost->colOffsets);
		CDTW_cost->colOffsets = malloc(sizeof(int)*(window->maxJ+1));
		if(!CDTW_cost->colOffsets)	
		{
			agios_print("PANIC! Could not allocate memory for constrained DTW cost matrix");
			max_constrainedDTW_cost_columns = max_constrainedDTW_cost_points =0;
			return NULL;
		}
		max_constrainedDTW_cost_columns = window->maxJ+1;
	}
	//cellValues will have the values for the cells listed by the window, so it will have length window->size
	if(max_constrainedDTW_cost_points < window->size)
	{	
		if(CDTW_cost->cellValues)
			free(CDTW_cost->cellValues);
		CDTW_cost->cellValues = malloc(sizeof(long long int)*window->size);
		if(!CDTW_cost->cellValues)
		{
			agios_print("PANIC! Could not allocate memory for constrained DTW cost matrix");
			max_constrainedDTW_cost_columns = max_constrainedDTW_cost_points =0;
			return NULL;
		}
		max_constrainedDTW_cost_points = window->size;	
	}
	//set right values at the structure
	CDTW_cost->cellValues_size = 0;
	CDTW_cost->colOffsets_size = window->maxJ+1;
	for(i = 0; i<= window->maxJ; i++)
	{
		CDTW_cost->colOffsets[i] = currentOffset;
		currentOffset += window->ranges[i].max - window->ranges[i].min + 1;	
	}

	return CDTW_cost;
}
void add_to_constrainedDTWcost(struct constrainedDTW_cost_t *cost, struct search_window_t *window, int row, int col, long long int value)
{
/*	if((row < window->ranges[col].min) || (row > window->ranges[col].max))
	{
		agios_print("PANIC! Trying to add a cell to constrained DTW cost which is out of the limits given by the window.\n");
		//now what?
	}
	else*/
	assert((row >= window->ranges[col].min) && (row <= window->ranges[col].max));
	assert(value >= 0);
	cost->cellValues_size++;
	cost->cellValues[cost->colOffsets[col]+row-window->ranges[col].min] = value;
}
long long int get_from_constrainedDTWcost(struct constrainedDTW_cost_t *cost, struct search_window_t *window, int row, int col)
{
	if((row < window->ranges[col].min) || (row > window->ranges[col].max))
		return LLONG_MAX;
	else
		return cost->cellValues[cost->colOffsets[col]+row - window->ranges[col].min];
}
//=====================================================================================================================================
//The fast DTW function will be recursively applied to shrunk versions of the original access patterns. The result of each version generates a window, which is used in this function below to get the result for the immediately larger ones.
struct time_warp_info_t *DTW_constrainedTimeWarp(struct access_pattern_t *tsI, struct access_pattern_t *tsJ, struct search_window_t *window)
{
//	PRINT_FUNCTION_NAME;
	int colu;
	int i, j;
	long long int diagCost, leftCost, downCost, mincost;
	struct time_warp_info_t *ret = Check_TimeWarpInfo_allocation(tsI->reqnb, tsJ->reqnb);
 
	if(ret == NULL)
	{
		agios_print("PANIC! Could not allocate memory for DTW\n");
		return NULL;
	}

	struct constrainedDTW_cost_t *costMatrix = Check_constrainedDTWcost_allocation(window);
	if(costMatrix == NULL) 
	{
		agios_print("PANIC! Could not apply DTW\n");
		return NULL;
	}
	int maxI = tsI->reqnb-1;
	int maxJ = tsJ->reqnb-1;

	//get an iterator that will list all cells in the window	
	struct search_window_iterator_t *matrixIterator = SearchWindow_obtain(window);
	if(matrixIterator == NULL)
		return NULL;
	for(colu = 0; colu < matrixIterator->size; colu++)
	{
		//current cell being filled
		i = matrixIterator->points[colu].i;
		j = matrixIterator->points[colu].j;
		if((i == 0) && (j == 0)) //bottom left cell (first row and first column)
			mincost = 0;
		else if(j == 0)//first column
		{
			mincost = get_from_constrainedDTWcost(costMatrix, window, i-1,j);
		}
		else if(i == 0)//first row
		{
			mincost = get_from_constrainedDTWcost(costMatrix, window, i, j-1);
		}
		else//not first column or first row		
		{
			mincost = min(get_from_constrainedDTWcost(costMatrix, window, i-1, j), min(get_from_constrainedDTWcost(costMatrix, window, i-1,j-1), get_from_constrainedDTWcost(costMatrix, window, i,j-1)));
		}
		add_to_constrainedDTWcost(costMatrix, window, i, j, positive_sum(mincost,DTW_euclideanDist(tsI, tsJ, i, j)));
	} // end for
	//minimum cost is at (max i, max j)
	ret->distance = get_from_constrainedDTWcost(costMatrix, window, maxI, maxJ);
//	debug("found distance %lld\n", ret->distance);

	//find the warp path by searching the matrix from the solution at (max i, max j) to the beginning at (0,0). at each step move through the matrix 1 step left, down, or diagonal, whichever has the smallest cost. Favor diagonal moves and moves towards the i==j axis to break ties
	i = maxI;
	j = maxJ;
	Add_to_TimeWarp_Path(ret, i, j);
	
	while((i > 0) || (j > 0))
	{
		//find the costs of moving in all three possible directions (left, down, and diagonal (down and left at the same time)
		if((i > 0)&& (j > 0))
		{
			diagCost = get_from_constrainedDTWcost(costMatrix, window, i-1, j-1);
		}
		else
			diagCost = LLONG_MAX;
		if(i > 0)
		{
			leftCost =get_from_constrainedDTWcost(costMatrix, window, i-1,j);
		}
		else
			leftCost = LLONG_MAX;
		if(j > 0)
		{
			downCost = get_from_constrainedDTWcost(costMatrix, window, i, j-1);
		}
		else
			downCost = LLONG_MAX;
		//determine which direction to move in. Prefer moving diagonally and towards the i==j axis of the matrix if there are ties
		if((diagCost <=	leftCost) && (diagCost <= downCost) && (i > 0) && (j > 0))
		{
			i--;
			j--;
		}
		else if((leftCost < diagCost) && (leftCost < downCost) && (i > 0))
			i--;
		else if((downCost < diagCost) && (downCost < leftCost) && (j > 0))
			j--;
		else if((i <= j) && (j > 0)) //leftCost == rightCost > diagCost
			j--;
		else if(i > 0)
			i--;
		else
			j--;
		//add the current step to the warp path
		Add_to_TimeWarp_Path(ret, i, j);
	} //end while


//	PRINT_FUNCTION_EXIT;
	return ret;
}

//=========================================================================================================================================
//functions to manage a time_warp_info_t, used to return results from DTW. The only moment it will be really allocated is in the DTW_DynamicTimeWarp function, which is called by fast DTW for the smallest possible access pattern size (the end of the recursion). 
//to avoid allocating and freeing a lot of these structures, we'll just allocate one, before calculating anything, that is large enough and keep it. 
//since we don't know how large is enough, we may have to redo it sometimes
int max_timewarppath_len = 0;
struct time_warp_info_t *TWinfo = NULL;

struct time_warp_info_t * Check_TimeWarpInfo_allocation(int tsI_reqnb, int tsJ_reqnb)
{
	//if we don't have the structure allocated, we'll start by doing that
	if(!TWinfo)
	{
		TWinfo = malloc(sizeof(struct time_warp_info_t));
		if(!TWinfo)
		{
			agios_print("PANIC! Cannot allocate memory for DTW resulting path.\n");
			return NULL;
		}
		TWinfo->points=NULL;
	}
	//now we'll see if the path inside is allocated with the right len
	if(max_timewarppath_len < (tsI_reqnb + tsJ_reqnb -1)) //we don't have a structure large enough
	{
		max_timewarppath_len = (tsI_reqnb + tsJ_reqnb);
		//clear previous one
		if(TWinfo->points)
			free(TWinfo->points);
		//allocate a new one
		TWinfo->points = malloc(sizeof(struct DTW_point)*max_timewarppath_len);
		if(!TWinfo->points)
		{
			agios_print("PANIC! Cannot allocate memory for DTW resulting path.\n");
			max_timewarppath_len=0;
			return NULL;
		}	
	}
	//reset it
	TWinfo->index = 0; 
	TWinfo->max = tsI_reqnb + tsJ_reqnb - 1;
	return TWinfo;
}
//adds a point (a,b) to the beginning of the path
void Add_to_TimeWarp_Path(struct time_warp_info_t *info, int a, int b)
{
	int i;
//	PRINT_FUNCTION_NAME;
	info->index++;
	if(info->index < info->max)
	{
		if(info->index > 1)
		{
			//move everything one position to the right
			for(i = info->index; i> 0; i--)
			{
				info->points[i].i = info->points[i-1].i;
				info->points[i].j = info->points[i-1].j;
			}	
		}
		//insert the new point
		info->points[0].i = a;
		info->points[0].j = b;
	}
	else
	{
		agios_print("PANIC! Trying to add a point to a time warp path which is already full %d %d, max is %d, size is %d\n", a, b, info->max, info->max);
		//now what?
	}
}
//=========================================================================================================================================
//functions to handle the cost matrix, used by the DTW_DynamicTipeWarp function. We'll allocate it once and keep during calculations
int costMatrix_sizeI = 0;
int costMatrix_sizeJ = 0;
long long int **cMatrix = NULL;

long long int ** Check_cMatrix_allocation(int tsI_reqnb, int tsJ_reqnb)
{
	int i;

	if((costMatrix_sizeI < tsI_reqnb) | (costMatrix_sizeJ < tsJ_reqnb))
	{
		//too small, need to redo
		//first free the previous one
		if(cMatrix)
		{
			for(i=0; i < costMatrix_sizeI; i++)
			{
				if(cMatrix[i])
					free(cMatrix[i]);
			}
			free(cMatrix);
		}
		//now allocate a new one
		cMatrix = malloc(sizeof(long long int *)*(tsI_reqnb));
		if(cMatrix == NULL)
		{
			agios_print("PANIC! Could not allocate cost Matrix for DTW");
			return NULL;
		}
		for(i = 0; i < tsI_reqnb; i++)
		{
			cMatrix[i] = malloc(sizeof(long long int)*(tsJ_reqnb));
			if(cMatrix[i] == NULL)
			{
				agios_print("PANIC! Could not allocate cost Matrix for DTW");
				return NULL;
			}
		}
		costMatrix_sizeI = tsI_reqnb;
		costMatrix_sizeJ = tsJ_reqnb;
	}
	return cMatrix;
}
//=========================================================================================================================================
//this function creats a shrunk version of an access pattern (to call fast DTW recursivelly to it)
struct access_pattern_t * shrink_time_series(struct access_pattern_t *ts, int shrunkSize)
{
	int ptToReadFrom, ptToReadTo;
	double reducedPtSize = ((double)ts->reqnb)/((double)shrunkSize);

	struct access_pattern_t *ret;

	//sanity checks
	if(ts->reqnb <= 0)
	{
		agios_print("PANIC! Trying to shrink an empty access pattern???\n");
		return NULL; 
	}
	if((shrunkSize > ts->reqnb) || (shrunkSize <= 0))
	{
		agios_print("PANIC! Trying to shrink time series of size %d with non-sensical size %d\n", ts->reqnb, shrunkSize);
		return NULL;
	}

	//allocate new access pattern
	ret = malloc(sizeof(struct access_pattern_t));
	if(ret == NULL)
	{
		agios_print("PANIC! Could not allocate memory for shrunk access pattern for fast DTW\n");
		return NULL;
	}
	ret->time_series = malloc(sizeof(struct pattern_tracker_req_info_t)*(shrunkSize));
	ret->aggPtSize = malloc(sizeof(int)*(shrunkSize));
	if((ret->time_series == NULL) || (ret->aggPtSize == NULL))
	{
		agios_print("PANIC! Could not allocate memory for shrunk access pattern for DTW\n");
		if(ret->time_series)
			free(ret->time_series);
		if(ret->aggPtSize)
			free(ret->aggPtSize);
		free(ret);
		return NULL;
	}
	
	//fill the new shrunk access pattern
	ret->reqnb=0;
	ret->original_size = ts->reqnb;

	ptToReadFrom=0;
	while(ptToReadFrom < ts->reqnb)
	{
		ptToReadTo = round(reducedPtSize*((double)ret->reqnb+1.0))-1; //determine the end of the current range
		ret->aggPtSize[ret->reqnb] = ptToReadTo - ptToReadFrom+1;
		//combine information from these points to a single point in the new access pattern
		combine_access_pattern_points(&(ret->time_series[ret->reqnb]), ts, ptToReadFrom, ptToReadTo); 
		ret->reqnb++;
		//next window of points to average starts where the last window ended
		ptToReadFrom = ptToReadTo + 1;
	}
	
	return ret;
}

//=========================================================================================================================================
//functions to manage the search window, that is used to extrapolate results obtained for shrunk versions of the access patterns to the full access patterns. 
//originally a search window is created to each recursive call, but we'll keep a single one (trying to make it large enough) to avoid that
int SearchWindow_len = 0;
struct search_window_t *SearchWindow=NULL;

struct search_window_t * new_search_window(int tsIsize, int tsJsize)
{
	int i;
//	PRINT_FUNCTION_NAME;
//	debug("SearchWindow_sizeI is %u, SearchWindowIterator_size = %u\n", SearchWindow_sizeI, SearchWindowIterator_size);

	//the search window has, to each column (j) of a cost matrix, the visited points represented by minimum and maximum value of i.
	//maxJ reflects the last column (number of columns -1), the array of ranges will go from 0 to maxJ
	//size reflects the number of points/cells covered by the window

	//first of all, if we don't alredy have a search window, we'll allocate one
	if(!SearchWindow)
	{
		SearchWindow = malloc(sizeof(struct search_window_t));
		if(!SearchWindow)
		{
			agios_print("PANIC! Could not allocate memory for search window for DTW");
			return NULL;
		}
		SearchWindow->ranges=NULL;
	}
	//now we can check the length
	if(tsJsize > SearchWindow_len) //our current search window is not large enough
	{
		//free the current search window 
		if(SearchWindow->ranges)
			free(SearchWindow->ranges);
		//allocate a new one
		SearchWindow->ranges = malloc(sizeof(struct DTW_range)*(tsJsize));
		if(!SearchWindow->ranges)
		{
			agios_print("PANIC! Could not allocate memory for search window for DTW");
			SearchWindow_len=0;
			return NULL;
		}
		SearchWindow_len = tsJsize;
	}
	//reset the search window
	for(i=0; i< tsJsize; i++)
	{
		SearchWindow->ranges[i].min=-1;
		SearchWindow->ranges[i].max=-1;
	}
	SearchWindow->maxJ = tsJsize-1;
	SearchWindow->maxI = tsIsize-1;
	SearchWindow->size = 0; 
//	debug("SearchWindow_sizeI NOW is %u and SearchWindowIterator_size is %u\n", SearchWindow_sizeI, SearchWindowIterator_size);
	return SearchWindow;
}
//we mark a point of the matrix (row, col) as visited, updating the window to reflect the right ranges
void SearchWindow_markVisited(struct search_window_t *window, int row, int col)
{
//	if((row < 0) || (col < 0) || (row > window->maxI) || (col > window->maxJ))
//		return;
	if(window->ranges[col].min < 0) //first value entered in the column
	{
		window->ranges[col].min = row;
		window->ranges[col].max = row;
		window->size++;
	}
	else if (window->ranges[col].min > row) //minimum range in the column is expanded
	{
		window->size += (window->ranges[col].min - row);
		window->ranges[col].min = row;
	}
	else if (window->ranges[col].max < row) //maximum range in the column is expanded
	{
		window->size += (row - window->ranges[col].max);
		window->ranges[col].max = row;
	}
}
//=========================================================================================================================================
//function to get a search window iterator, which is a list of all points covered by a given window
//similarly to other data structures, we'll keep a previously allocated one
int SearchWindowIterator_len = 0;
struct search_window_iterator_t *SearchWindowIterator=NULL;

struct search_window_iterator_t *SearchWindow_obtain(struct search_window_t *window)
{
	//the length of the iterator is the number of points in the window, given by window->size

	//first we'll check the allocation
	if(!SearchWindowIterator)
	{
		SearchWindowIterator = malloc(sizeof(struct search_window_iterator_t));
		if(!SearchWindowIterator)
		{
			agios_print("PANIC! Could not allocate a search window iterator for DTW");
			return NULL;
		}
		SearchWindowIterator->points = NULL;
	}	
	if(SearchWindowIterator_len < window->size)
	{
		//we'll need a larger one
		if(SearchWindowIterator->points)
			free(SearchWindowIterator->points);
		SearchWindowIterator->points = malloc(sizeof(struct DTW_point)*window->size);
		if(!SearchWindowIterator->points)
		{
			agios_print("PANIC! Could not allocate a search window iterator for DTW");
			SearchWindowIterator_len=0;
			return NULL;
		}
		SearchWindowIterator_len=window->size;
	}

	//OK, now we can create our iterator
	int minvalue, maxvalue;
	int i;
	
	SearchWindowIterator->size=0; 
	for(i = 0; i <= window->maxJ; i++)
	{
		minvalue = window->ranges[i].min;
		/*if(minvalue < 0) 
		{
			agios_print("Warning: missing column %d in the search window", i);	
			continue;
		}*/
		assert((minvalue >= 0));
		maxvalue = window->ranges[i].max;
		while(minvalue <= maxvalue)
		{
			SearchWindowIterator->points[SearchWindowIterator->size].j = i;
			SearchWindowIterator->points[SearchWindowIterator->size].i = minvalue;
			SearchWindowIterator->size++;
			minvalue++;	
		}	
	}
	return SearchWindowIterator;
}
//=========================================================================================================================================
//functions that are used by fast dtw, after obtaining the result for the shrunk access patterns, to create and expand a window that will be used to generate results for the larger version of the access patterns 
void SearchWindow_expandSearchWindow(struct search_window_t *window, int radius)
{
	int cell;
	int targetCol, targetRow, cellsPastEdge;

//	PRINT_FUNCTION_NAME;

	if(radius > 0) //if radius <= 0 then no search is necessary, use the current search window
	{
		//add all cells in the current window to an array, iterating through the window and expanding the window
		struct search_window_iterator_t *windowCells = SearchWindow_obtain(window);
		if(windowCells == NULL)
			return;
/*		debug("our iterator:");
		for(i=0; i< windowCells->size; i++)
			printf("%d %d\t", windowCells->points[i].i,windowCells->points[i].j);
		printf("\n"); */
		for(cell = 0; cell < windowCells->size; cell++)
		{
			if((windowCells->points[cell].i != 0) && (windowCells->points[cell].j != window->maxJ)) //move to upper left is possible 
			{
				//either expand full search radius or some fraction until edges of matrix are met
				targetRow = windowCells->points[cell].i - radius;
				targetCol = windowCells->points[cell].j + radius;
				if((targetRow < 0) || (targetCol > window->maxJ))
				{
					//expand the window only to the edge of the matrix
					cellsPastEdge = max(0 - targetRow, targetCol - window->maxJ);
					targetRow=targetRow+cellsPastEdge;
					targetCol = targetCol-cellsPastEdge;
				}
				//assert((targetRow >= 0) && (targetCol >= 0) && (targetRow <= window->maxI) && (targetCol <= window->maxJ));
				SearchWindow_markVisited(window, targetRow, targetCol);
			} 
			if(windowCells->points[cell].j != window->maxJ) //move up if possible
			{
				//either extend full search radius or some fraction until edges of matrix are met
				targetRow = windowCells->points[cell].i;
				targetCol = windowCells->points[cell].j + radius;
			
				if(targetCol > window->maxJ)
					targetCol = window->maxJ;
				//assert((targetRow >= 0) && (targetCol >= 0) && (targetRow <= window->maxI) && (targetCol <= window->maxJ));
				SearchWindow_markVisited(window, targetRow, targetCol); 
			}
			if((windowCells->points[cell].i != window->maxI) && (windowCells->points[cell].j != window->maxJ)) //move to upper-right if possible
			{
				//either extend full search radius or some fraction until edges of matrix are met
				targetRow = windowCells->points[cell].i + radius;
				targetCol = windowCells->points[cell].j + radius;
				if ((targetRow > window->maxI) || (targetCol > window->maxJ))
				{
					//expand the window only to the edge of the matrix
					cellsPastEdge = max(targetRow - window->maxI,targetCol - window->maxJ);
					targetRow = targetRow-cellsPastEdge;
					targetCol = targetCol - cellsPastEdge;
				}
				//assert((targetRow >= 0) && (targetCol >= 0) && (targetRow <= window->maxI) && (targetCol <= window->maxJ));
				SearchWindow_markVisited(window, targetRow, targetCol); 
			}
			if(windowCells->points[cell].i != 0) // move left if possible
			{
				//either extend full search radius or some fraction until edges of matrix are met
				targetRow = windowCells->points[cell].i - radius;
				targetCol = windowCells->points[cell].j;
				if(targetRow < 0)
					targetRow = 0;
				//assert((targetRow >= 0) && (targetCol >= 0) && (targetRow <= window->maxI) && (targetCol <= window->maxJ));
				SearchWindow_markVisited(window, targetRow, targetCol); 
			}
			if(windowCells->points[cell].i != window->maxI) //move right if possible
			{
				targetRow = windowCells->points[cell].i+radius;
				targetCol = windowCells->points[cell].j;
				if(targetRow > window->maxI)
					targetRow = window->maxI;
				//assert((targetRow >= 0) && (targetCol >= 0) && (targetRow <= window->maxI) && (targetCol <= window->maxJ));
				SearchWindow_markVisited(window, targetRow, targetCol); 
			}
			if((windowCells->points[cell].i != 0) && (windowCells->points[cell].j != 0)) //move to lower-left is possible
			{
				targetRow = windowCells->points[cell].i-radius;
				targetCol = windowCells->points[cell].j-radius;
				if((targetCol < 0) || (targetRow < 0))
				{
					cellsPastEdge = min(targetCol, targetRow); 
					targetRow = targetRow - cellsPastEdge;
					targetCol = targetCol - cellsPastEdge;
				}
				//assert((targetRow >= 0) && (targetCol >= 0) && (targetRow <= window->maxI) && (targetCol <= window->maxJ));
				SearchWindow_markVisited(window, targetRow, targetCol); 
			} 
			if (windowCells->points[cell].j != 0) //move down if possible
			{
				targetRow = windowCells->points[cell].i;
				targetCol = windowCells->points[cell].j-radius;
				if(targetCol < 0)
					targetCol = 0;
				//assert((targetRow >= 0) && (targetCol >= 0) && (targetRow <= window->maxI) && (targetCol <= window->maxJ));
				SearchWindow_markVisited(window, targetRow, targetCol); 
			}
			if((windowCells->points[cell].i != window->maxI) && (windowCells->points[cell].j != 0)) //move to lower-right if possible
			{
				targetRow = windowCells->points[cell].i + radius;
				targetCol = windowCells->points[cell].j - radius;
				if((targetRow > window->maxI) || (targetCol < 0))
				{
					cellsPastEdge = max(targetRow - window->maxI, 0 - targetCol);
					targetRow = targetRow - cellsPastEdge;
					targetCol = targetCol + cellsPastEdge;
				}
				//assert((targetRow >= 0) && (targetCol >= 0) && (targetRow <= window->maxI) && (targetCol <= window->maxJ));
				SearchWindow_markVisited(window, targetRow, targetCol); 
			}
		}//end for
	} //end if
}

void SearchWindow_expandWindow(struct search_window_t *window, int radius)
{
	if(radius > 0)
	{
		SearchWindow_expandSearchWindow(window, 1);
		SearchWindow_expandSearchWindow(window, radius-1);
	}
}

struct search_window_t *SearchWindow_ExpandedResWindow(struct access_pattern_t *tsI, struct access_pattern_t *tsJ, struct access_pattern_t *shrunkI, struct access_pattern_t *shrunkJ, struct time_warp_info_t *path, int searchRadius)
{
//	PRINT_FUNCTION_NAME;
	int w, warpedI, warpedJ, blockIsize, blockJsize, x, y;
	//variables to keep track of the current location of the higher resolution projected path
	int currentI = path->points[0].i;
	int currentJ = path->points[0].j;
	//variables to keep track of the last part of the low-resolution warp path that was evaluated (to determine direction
	int lastWarpedI = INT_MAX;
	int lastWarpedJ = INT_MAX;
	struct search_window_t *ret = new_search_window(tsI->reqnb,tsJ->reqnb);
	if(ret == NULL)
	{
		return NULL;
	}
	//for each part of the low-resolution warp path, project that path to the higher resolution by filling in the path's corresponding cells at the higher resolution
	for(w = 0; w < path->index; w++)
	{
		warpedI = path->points[w].i;
		warpedJ = path->points[w].j;
		blockIsize = shrunkI->aggPtSize[warpedI];
		blockJsize = shrunkJ->aggPtSize[warpedJ];
		//if the path moved up or diagonally, then the next cell's values on the J axis will be larger
		if(warpedJ > lastWarpedJ)
		{
			currentJ += shrunkJ->aggPtSize[lastWarpedJ];	
		}
		if(warpedI > lastWarpedI)
		{
			currentI += shrunkI->aggPtSize[lastWarpedI];
		}
		//if a diagonal move was performed, add 2 cells to the edges of the 2 blocks in the projected path to create a continuous path (path with even width... avoid a path of boxes connected only at their corners
	 	 //                        |_|_|x|x|     then mark      |_|_|x|x|
	         //    ex: projected path: |_|_|x|x|  --2 more cells->  |_|X|x|x|
	         //                        |x|x|_|_|        (X's)       |x|x|X|_|
	         //                        |x|x|_|_|                    |x|x|_|_|
		if((warpedJ > lastWarpedJ) & (warpedI > lastWarpedI))
		{
			SearchWindow_markVisited(ret, currentI-1, currentJ);
			SearchWindow_markVisited(ret, currentI, currentJ-1);
		}
		//fill in the cells that are created by a projection from the cell in the low-resolution warp path to a higher resolution
		for(x = 0; x < blockIsize; x++)
		{
			//SearchWindow_markVisited(ret, currentI+x, currentJ);
			//SearchWindow_markVisited(ret, currentI+x, currentJ + blockJsize-1);
			for(y=0; y < blockJsize; y++)
			{
				SearchWindow_markVisited(ret, currentI+x, currentJ+y);
			}
		}
		//record the last position in the time warp path so the direction of the path can be determined when the next position of the path is evaluated
		lastWarpedI = warpedI;
		lastWarpedJ = warpedJ;
	}
/*	debug("our path:");
	for(i=0; i<path->index; i++)
		printf("%d %d\t", path->points[i].i, path->points[i].j);
	printf("\n");
	debug("aggptsizes for I:");
	for(i=0; i< shrunkI->reqnb; i++)
		printf("[%d]: %d\t", i, shrunkI->aggPtSize[i]);
	printf("\n");
	debug("aggptsizes for J:");
	for(i=0; i< shrunkJ->reqnb; i++)
		printf("[%d]: %d\t", i, shrunkJ->aggPtSize[i]);
	printf("\n");
	
	debug("our window:");
	for(i=0; i<= ret->maxJ; i++)
		printf("%d %d\t", ret->ranges[i].min, ret->ranges[i].max);
	printf("\n");*/
	//expand the size of the projected warp path by the specified width
	SearchWindow_expandWindow(ret, searchRadius);

//	PRINT_FUNCTION_EXIT;
	return ret;
}
//=========================================================================================================================================
//Fast DTW 
struct time_warp_info_t *FastDTW_fastDTW(struct access_pattern_t *tsI, struct access_pattern_t *tsJ, int searchRadius)
{
	int minTSsize;
	struct search_window_t *window;
	struct access_pattern_t *shrunkI, *shrunkJ;
	struct time_warp_info_t *ret;

//	PRINT_FUNCTION_NAME;

	if(searchRadius < 0)
		searchRadius = 0;

	minTSsize = searchRadius+2;
	if((tsI->reqnb <= minTSsize) || (tsJ->reqnb <= minTSsize))
	{
		//perform full DTW
		return DTW_DynamicTimeWarp(tsI, tsJ, 0); 
	}		
	else
	{
		shrunkI = shrink_time_series(tsI, (tsI->reqnb / 2));
		shrunkJ = shrink_time_series(tsJ, (tsJ->reqnb / 2));
		if((shrunkI == NULL) || (shrunkJ == NULL))
		{
			agios_print("PANIC! Could not shrink time series from access patterns\n");
			return NULL;
		}

		//determine the search window that constrains the area of the cost matrix that will be evaluated based on the warp path found at the previous resolution (smaller time series)
		ret = FastDTW_fastDTW(shrunkI, shrunkJ, searchRadius);
		if(ret)
		{
			window = SearchWindow_ExpandedResWindow(tsI, tsJ, shrunkI, shrunkJ, ret, searchRadius);
			if(window)
			{
			/*	printf("our window is: ");
				for(i = 0; i<= window->maxJ; i++)
					printf("%d, %d\t", window->ranges[i].min, window->ranges[i].max);
				printf("\n");*/
				//find the optimal path through this search window constraint
				ret = DTW_constrainedTimeWarp(tsI, tsJ, window);
			}
			else
				ret = NULL;	
		}
		//clean the access patterns we've created here
		if(shrunkI)
		{
			if(shrunkI->time_series)
				free(shrunkI->time_series);
			if(shrunkI->aggPtSize)
				free(shrunkI->aggPtSize);
			free(shrunkI);
		}
		if(shrunkJ)
		{
			if(shrunkJ->time_series)
				free(shrunkJ->time_series);
			if(shrunkJ->aggPtSize)
				free(shrunkJ->aggPtSize);
			free(shrunkJ);
		}		

		return ret;
	}
}
// This is the function called from outside to compare two access patterns A and B. It will return an long long int that is a score. The higher the score, the higher the distance between the patterns (the more different they are). 
long long int FastDTW(struct access_pattern_t *A, struct access_pattern_t *B)
{
	long long int ret = LLONG_MAX;
	short int error=1;

	//before calling the fast dtw function, well allocate some structures using the largest size possible for this calculation, so we won't have to reallocate logN times during it
	if(Check_TimeWarpInfo_allocation(A->reqnb, B->reqnb))
	{
		if(new_search_window(A->reqnb,B->reqnb))
		{
			error=0;
		}
	}
	if(error)
	{
		agios_print("PANIC! Could not allocate memory for DTW");
		return ret;
	}
	//we won't allocate CDTW_cost now because its size depends on the number of cells, which could be reqnb^2, but is usually way less than that (but we won't know until we have a search window filled
	//we won't allocate cMatrix because it is only used by the full DTW, which will be called for very small patterns (and only once during the execution)
	//we won't allocate the search window iterator now either because, just like CDTW_cost, it depends on the number of points in the window
	
	struct time_warp_info_t *info = FastDTW_fastDTW(A, B, 1); 
	if(info)
	{
		ret = info->distance;
	}
	else
	{
	//	debug("error @ FastDTW\n");
		agios_print("PANIC! Could not apply fast DTW");
	}
	return ret;
}
//=========================================================================================================================================
// function called from outside of this module before finishing the execution, to clean up all allocated data structures
void DTW_cleanup()
{
//	debug("TWinfo has size %d, SearchWindow %d CDTW_cost %d points %d columns, cMatrix %d x %d, SearchWindowIterator %d", max_timewarppath_len, SearchWindow_len, max_constrainedDTW_cost_points, max_constrainedDTW_cost_columns, costMatrix_sizeI, costMatrix_sizeJ, SearchWindowIterator_len);
	//clean the time_warp_info_t
	if(TWinfo)
	{
		if(TWinfo->points)
			free(TWinfo->points);
		free(TWinfo);
	}
	//clean the search window
	if(SearchWindow)
	{
		if(SearchWindow->ranges)
			free(SearchWindow->ranges);
		free(SearchWindow);
	}
	//clean the constrainedDTW_cost_t
	if(CDTW_cost)
	{
		if(CDTW_cost->cellValues)
			free(CDTW_cost->cellValues);
		if(CDTW_cost->colOffsets)
			free(CDTW_cost->colOffsets);
		free(CDTW_cost);
	}
	//free the DTW cost matrix
	if(cMatrix)
	{
		int i;
		for(i=0; i < costMatrix_sizeI; i++)
		{
			if(cMatrix[i])
				free(cMatrix[i]);
		}
		free(cMatrix);
	}
	//clean the search window iterator
	if(SearchWindowIterator)
	{
		if(SearchWindowIterator->points)
			free(SearchWindowIterator->points);
		free(SearchWindowIterator);
	}

}

