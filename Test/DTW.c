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
 *  Translated to C and adapted and optimized by Francieli Zanon Boito - Federal University of Santa Catarina (Brazil)
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
 *		Further information is available at http://agios.bitbucket.org/
 *
 * Contributors:
 *		Federal University of Rio Grande do Sul (UFRGS)
 *		INRIA France
 *		Federal University of Santa Catarina (UFSC)
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
//#include "common_functions.h"
#include "DTW.h"


#define agios_print(f, a...) 					fprintf(stderr, "AGIOS: " f "\n", ## a)

//some functions were kepts, although they are not useful (as the two functions below) so the mapping between the C++ and the C versions would be clear (they could be removed later, but the performance impact should be minimal)
inline struct time_warp_info_t * DTW_getWarpInfoBetween(struct access_pattern_t *tsI, struct access_pattern_t *tsJ)
{
	return DTW_DynamicTimeWarp(tsI, tsJ);
} 
inline struct time_warp_info_t *DTW_getWarpInfoBetween_withWindow(struct access_pattern_t *tsI, struct access_pattern_t *tsJ, struct search_window_t *window)
{
	return DTW_constrainedTimeWarp(tsI, tsJ, window);
}

//=========================================================================================================================================
//functions to manage - allocate, access, free - a memory_resident_matrix_t structure, used by constrained DTW. Returns NULL on error
//the name does not seem to mean anything, it was kept so the mapping between the different versions of the code would make sense
//originally a new matrix was allocated every time the constrainedTimeWarp function was called, but we'll try to avoid that since we call this function too often (maybe keeping the burden in memory is the best choice)
//the problem is that we don't know how large we'll need the window to be. So we'll reallocate it a few times until we get a large enough one
int max_window_size=0;
struct memory_resident_matrix_t *MRmatrix=NULL;

inline void allocate_memory_resident_matrix_t(int size);

inline struct memory_resident_matrix_t *get_new_memory_resident_matrix_t(struct search_window_t *window)
{
	int i;
	int currentOffset=0;

	if(max_window_size == 0) //we did not allocate the matrix yet
	{
		max_window_size = window->size*1.1; //we allocate a little more than what we need right now so maybe we won't have to reallocate it so many times
		allocate_memory_resident_matrix_t(max_window_size); 
		if((!MRmatrix) | (!MRmatrix->cellValues) | (!MRmatrix->colOffsets)) //could not allocate, already printed error message
		{
			max_window_size = 0;
			return NULL;
		}
	}
	else if(max_window_size < window->size) //our matrix is too small, we need to free it and then allocate it again
	{
		free(MRmatrix->cellValues);
		free(MRmatrix->colOffsets);
		max_window_size = window->size*1.1; //we allocate a little more than what we need right now so maybe we won't have to reallocate it so many times
		allocate_memory_resident_matrix_t(max_window_size);
		if((!MRmatrix) | (!MRmatrix->cellValues) | (!MRmatrix->colOffsets)) //could not allocate, already printed error message
		{
			max_window_size = 0;
			return NULL;
		}
	}
	//now we know we have a matrix large enough to use, just have to reset it
	MRmatrix->cellValues_size=0;
	MRmatrix->colOffsets_size=0;

	for(i = 0; i < window->size; i++)
	{
		MRmatrix->colOffsets[i] = currentOffset;
		MRmatrix->colOffsets_size++;
		currentOffset += window->maxValues[i] - window->minValues[i] + 1;
	}
	return MRmatrix;	
}
inline void allocate_memory_resident_matrix_t(int size)
{
	if(MRmatrix == NULL) //no need to reallocate the struct everytime, only the vectors
	{
		MRmatrix = malloc(sizeof(struct memory_resident_matrix_t));
		if(MRmatrix == NULL)
		{
			agios_print("PANIC! Could not allocate memory for DTW\n");
			return;
		}
	}
	MRmatrix->cellValues = malloc(sizeof(unsigned long long int)*(size)); 
	if(MRmatrix->cellValues == NULL)
	{
		agios_print("PANIC! Could not allocate memory for DTW\n");
		return;
	}
	MRmatrix->colOffsets = (int *)malloc(sizeof(int)*(size));
	if(MRmatrix->colOffsets == NULL)
	{
		agios_print("PANIC! Could not allocate memory for DTW\n");
		return;
	}
}
inline void add_to_memory_resident_matrix_t(struct memory_resident_matrix_t *matrix, struct search_window_t *window, int col, int row, unsigned long long int value)
{
	if((row < window->minValues[col]) || (row > window->maxValues[col]))
	{
		agios_print("PANIC! Trying to add a cell to memory resident matrix which is out of the limits given by the window.\n");
		//now what?
	}
	else
		matrix->cellValues[matrix->colOffsets[col]+row-window->minValues[col]] = value;
}
inline unsigned long long int get_from_memory_resident_matrix_t(struct memory_resident_matrix_t *matrix, struct search_window_t *window, int col, int row)
{
	if((row < window->minValues[col]) || (row > window->maxValues[col]))
		return ULLONG_MAX;
	else
		return matrix->cellValues[matrix->colOffsets[col]+row - window->minValues[col]];
}

//=========================================================================================================================================
//The fast DTW function will be recursively applied to shrunk versions of the original access patterns. The result of each version generates a window, which is used in this function below to get the result for the immediately larger ones.
inline struct time_warp_info_t * Check_TimeWarpInfo_allocation(unsigned int tsI_reqnb, unsigned int tsJ_reqnb);
struct time_warp_info_t *DTW_constrainedTimeWarp(struct access_pattern_t *tsI, struct access_pattern_t *tsJ, struct search_window_t *window)
{
	unsigned int colu, i, j;
	unsigned long long int diagCost, leftCost, downCost;
//	struct time_warp_info_t *ret = malloc(sizeof(struct time_warp_info_t)); 
	struct time_warp_info_t *ret = Check_TimeWarpInfo_allocation(tsI->reqnb, tsJ->reqnb);
 
	if(ret == NULL)
	{
		agios_print("PANIC! Could not allocate memory for DTW\n");
		return NULL;
	}

	//     COST MATRIX:
	//   5|_|_|_|_|_|_|E| E = min Global Cost
	//   4|_|_|_|_|_|_|_| S = Start point
	//   3|_|_|_|_|_|_|_| each cell = min global cost to get to that point
	// j 2|_|_|_|_|_|_|_|
	//   1|_|_|_|_|_|_|_|
	//   0|S|_|_|_|_|_|_|
	//     0 1 2 3 4 5 6
	//            i
	//   access is M(i,j)... column-row
	struct memory_resident_matrix_t *costMatrix = get_new_memory_resident_matrix_t(window);
	if(costMatrix == NULL) 
	{
		agios_print("PANIC! Could not apply DTW\n");
		return NULL;
	}
	int maxI = tsI->reqnb-1;
	int maxJ = tsJ->reqnb-1;
	
	//get an iterator that traverses the window cells in the order that the cost matrix is filled (first to last row (1...maxI), bottom to top (1...maxJ)
	struct search_window_t *matrixIterator = SearchWindow_obtain(window);
	if(matrixIterator == NULL)
		return NULL;
	for(colu = 0; colu < matrixIterator->size; colu++)
	{
		//current cell being filled
		i = matrixIterator->minValues[colu];
		j = matrixIterator->maxValues[colu];
		if((i == 0) && (j == 0)) //bottom left cell (first row and first column)
			add_to_memory_resident_matrix_t(costMatrix, window, i, j, DTW_euclideanDist(tsI->time_series[0].offset, tsJ->time_series[0].offset));
		else if (i == 0) //first column
			add_to_memory_resident_matrix_t(costMatrix, window, i, j, DTW_euclideanDist(tsI->time_series[0].offset, tsJ->time_series[j].offset) + get_from_memory_resident_matrix_t(costMatrix, window, i, j-1));
		else if (j == 0) //first row
			add_to_memory_resident_matrix_t(costMatrix, window, i, j, DTW_euclideanDist(tsI->time_series[i].offset, tsJ->time_series[0].offset) + get_from_memory_resident_matrix_t(costMatrix, window, i-1,j));
		else //not first column or first row		
			add_to_memory_resident_matrix_t(costMatrix, window, i, j, fmin(get_from_memory_resident_matrix_t(costMatrix, window, i-1, j), fmin(get_from_memory_resident_matrix_t(costMatrix, window, i-1,j-1), get_from_memory_resident_matrix_t(costMatrix, window, i,j-1))) + DTW_euclideanDist(tsI->time_series[i].offset, tsJ->time_series[j].offset));
	} // end for
	//minimum cost is at (max i, max j)
	ret->distance = get_from_memory_resident_matrix_t(costMatrix, window, maxI, maxJ);

	//find the warp path by searching the matrix from the solution at (max i, max j) to the beginning at (0,0). at each step move through the matrix 1 step left, down, or diagonal, whichever has the smallest cost. Favor diagonal moves and moves towards the i==j axis to break ties
//	Initialize_TimeWarp_Path(&ret->path, (maxI + maxJ - 1));
	i = maxI;
	j = maxJ;
	Add_to_TimeWarp_Path(&ret->path, i, j);
	
	while((i > 0) || (j > 0))
	{
		//find the costs of moving in all three possible directions (left, down, and diagonal (down and left at the same time)
		if((i > 0)&& (j > 0))
			diagCost = get_from_memory_resident_matrix_t(costMatrix, window, i-1, j-1);
		else
			diagCost = ULLONG_MAX;
		if(i > 0)
			leftCost = get_from_memory_resident_matrix_t(costMatrix, window, i-1,j);
		else
			leftCost = ULLONG_MAX;
		if(j > 0)
			downCost = get_from_memory_resident_matrix_t(costMatrix, window, i, j-1);
		else
			downCost = ULLONG_MAX;
		//determine which direction to move in. Prefer moving diagonally and towards the i==j axis of the matrix if there are ties
		if((diagCost <=	leftCost) && (diagCost <= downCost))
		{
			i--;
			j--;
		}
		else if((leftCost < diagCost) && (leftCost < downCost))
			i--;
		else if((downCost < diagCost) && (downCost < leftCost))
			j--;
		else if(i <= j) //leftCost == rightCost > diagCost
			j--;
		else
			i--;
		//add the current step to the warp path
		Add_to_TimeWarp_Path(&ret->path, i, j);
	} //end while


	return ret;
}

//=========================================================================================================================================
//functions to manage a time_warp_info_t, used to return results from DTW. The only moment it will be really allocated is in the DTW_DynamicTimeWarp function, which is called by fast DTW for the smallest possible access pattern size (the end of the recursion). 
//to avoid allocating and freeing a lot of these structures, we'll just allocate one, before calculating anything, that is large enough and keep it. 
//since we don't know how large is enough, we may have to redo it sometimes
int max_timewarppath_len = -1;
struct time_warp_info_t *TWinfo = NULL;

inline void Initialize_TimeWarp_Path(struct warp_path_t *new, unsigned long int size);

inline struct time_warp_info_t * Check_TimeWarpInfo_allocation(unsigned int tsI_reqnb, unsigned int tsJ_reqnb)
{
	if((max_timewarppath_len == -1) || (max_timewarppath_len < (tsI_reqnb + tsJ_reqnb -1))) //we don't have a structure large enough
	{
		max_timewarppath_len = (tsI_reqnb + tsJ_reqnb -1)*1.1;
		//clear previous one
		if(TWinfo)
		{
			if(TWinfo->path.tsIindexes)
				free(TWinfo->path.tsIindexes);
			if(TWinfo->path.tsJindexes)
				free(TWinfo->path.tsJindexes);
		}
		//allocate a new one
		if(!TWinfo)
		{
			TWinfo = malloc(sizeof(struct time_warp_info_t));
			if(!TWinfo)
			{
				agios_print("PANIC! Cannot allocate memory for DTW.\n");
				max_timewarppath_len = -1;
				return NULL;
			}
		}
		Initialize_TimeWarp_Path(&(TWinfo->path), max_timewarppath_len);
	}
	//reset it
	TWinfo->path.index = 0; //reset it
	TWinfo->path.max = tsI_reqnb + tsJ_reqnb - 1;
	return TWinfo;
}
inline void Initialize_TimeWarp_Path(struct warp_path_t *new, unsigned long int size)
{
	new->tsIindexes = malloc(sizeof(int)*size);
	new->tsJindexes = malloc(sizeof(int)*size);
	if((new->tsIindexes == NULL) || (new->tsJindexes == NULL))
	{	
		agios_print("PANIC! Cannot allocate memory for DTW.\n");
		//now what?
	}
}
//adds a point (a,b) to the beginning of the path
inline void Add_to_TimeWarp_Path(struct warp_path_t *path, int a, int b)
{
	int i;
	path->index++;
	if(path->index < path->max)
	{
		if(path->index > 1)
		{
			//move everything one position to the right
			for(i = path->index; i> 0; i++)
			{
				path->tsIindexes[i] = path->tsIindexes[i-1];	
				path->tsJindexes[i] = path->tsJindexes[i-1];
			}	
		}
		//insert the new point
		path->tsIindexes[0] = a;
		path->tsJindexes[0] = b;
	}
	else
	{
		agios_print("PANIC! Trying to add a point to a time warp path which is already full\n");
		//now what?
	}
}
//=========================================================================================================================================
//functions to handle the cost matrix, used by the DTW_DynamicTipeWarp function. We'll allocate it once and keep during calculations
int costMatrix_sizeI = -1;
int costMatrix_sizeJ = -1;
unsigned long long int **cMatrix = NULL;

inline unsigned long long int ** Check_cMatrix_size(unsigned int tsI_reqnb, unsigned int tsJ_reqnb)
{
	int i;

	if((costMatrix_sizeI <= tsI_reqnb) | (costMatrix_sizeJ <= tsJ_reqnb))
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
		cMatrix = malloc(sizeof(unsigned long long int *)*(tsI_reqnb+1));
		if(cMatrix == NULL)
		{
			agios_print("PANIC! Could not allocate memory for DTW");
			return NULL;
		}
		for(i = 0; i < tsI_reqnb; i++)
		{
			cMatrix[i] = malloc(sizeof(unsigned long long int)*(tsJ_reqnb+1));
			if(cMatrix[i] == NULL)
			{
				agios_print("PANIC! Could not allocate memory for DTW");
				return NULL;
			}
		}
		costMatrix_sizeI = tsI_reqnb+1;
		costMatrix_sizeJ = tsJ_reqnb+1;
	}
	return cMatrix;
}
//=========================================================================================================================================
//function to perform full DTW between two access patterns, called by the fast DTW in the smallest case (end of recursion)
struct time_warp_info_t * DTW_DynamicTimeWarp(struct access_pattern_t *tsI, struct access_pattern_t *tsJ)
{
	int i,j;
	struct time_warp_info_t *ret = Check_TimeWarpInfo_allocation(tsI->reqnb, tsJ->reqnb);
	if(ret == NULL)
	{
		agios_print("PANIC! Could not allocate memory for DTW");
		return NULL;
	}

	//allocate a cost matrix
	unsigned long long int **costMatrix = Check_cMatrix_size(tsI->reqnb, tsJ->reqnb);
	if(!costMatrix)
		return NULL;
	for(i = 0; i < tsI->reqnb; i++)
	{
		for(j=0; j < tsJ->reqnb; j++)
			costMatrix[i][j] = 0;
	}

	//calculate the values for the first column, from the bottom up
	costMatrix[0][0] = DTW_euclideanDist(tsI->time_series[0].offset, tsJ->time_series[0].offset);
	for(j = 1; j < tsJ->reqnb; j++)
		costMatrix[0][j] = costMatrix[0][j-1] + DTW_euclideanDist(tsI->time_series[0].offset, tsJ->time_series[j].offset);
	//now for the rest of the matrix
	for(i = 1; i< tsI->reqnb; i++) 
	{
		//calculate for the bottom row
		costMatrix[i][0] = costMatrix[i-1][0] + DTW_euclideanDist(tsI->time_series[i].offset, tsJ->time_series[0].offset);
		//now for the other rows
		for(j = 1; j < tsJ->reqnb; j++)
			costMatrix[i][j] = (fmin(costMatrix[i-1][j], fmin(costMatrix[i-1][j-1], costMatrix[i][j-1]))) + DTW_euclideanDist(tsI->time_series[i].offset, tsJ->time_series[j].offset);
	}
	//ok, now we have minimum cost
	ret->distance = costMatrix[tsI->reqnb-1][tsJ->reqnb-1];

	//find the warp path by searching the matrix form the solution to at (maxI, maxJ) to the beginning at (0,0). At each step move through the matrix 1 step left, down, or diagonal, whichever has the smallest cost. Favor diagonal moves and moves towards the i==j axis to break ties.
	i = tsI->reqnb-1;
	j = tsJ->reqnb-1;
	Add_to_TimeWarp_Path(&ret->path, i, j);
	while((i > 0) || (j > 0))
	{
		//find the costs of moving in all three possible directions (left, down, and diagonal (down and left at the same time)
		unsigned long long int diagCost, leftCost, downCost;
		
		if(( i > 0) & (j > 0))
			diagCost = costMatrix[i-1][j-1];
		else
			diagCost = ULLONG_MAX;
		if (i > 0)
			leftCost = costMatrix[i-1][j];
		else
			leftCost = ULLONG_MAX;
		if(j > 0)
			downCost = costMatrix[i][j-1];
		else
			downCost = ULLONG_MAX;
		//determine which direction to move in. Prefer moving diagonally and moving towards i == j axis of the matrixif there are ties
		if((diagCost <= leftCost) && (diagCost <= downCost))
		{
			i--;
			j--;
		}
		else if ((leftCost <diagCost) && (leftCost < downCost))
			i--;
		else if ((downCost < diagCost) && (downCost < leftCost))
			j--;
		else if (i <= j) //leftCost == rightCost > diagCost
			j--;
		else 
			i--;
		//add the current step to the warp path
		Add_to_TimeWarp_Path(&ret->path, i, j);
	}


	return ret;
}

inline unsigned long long int DTW_euclideanDist(long long int value1, long long int value2)
{
//	return pow(sqrt(pow(tsI->time_series[indexI].offset - tsJ->time_series[indexJ].offset, 2.0)), 2.0);
//actually this was to compare vectors, since we are using 1d dtw, we will only compare pairs of values, so:
	return abs(value1-value2);
}

//the function that created a shrunk version of an access pattern (to call fast DTW recursivelly to it)
struct access_pattern_t * shrink_time_series(struct access_pattern_t *ts, int shrunkSize)
{
	int ptToReadFrom, ptToReadTo, pt;
	long long int sum_offsets;
	unsigned long long int sum_timestamps;
	double reducedPtSize = ((double)ts->reqnb)/((double)shrunkSize);

	struct access_pattern_t *ret;

	if(ts->reqnb <= 0)
	{
		agios_print("PANIC! Trying to shrink an empty access pattern???\n");
		return NULL; 
	}

	ret = malloc(sizeof(struct access_pattern_t));
	if(ret == NULL)
	{
		agios_print("PANIC! Could not allocate memory for DTW\n");
		return NULL;
	}
	ret->time_series = malloc(sizeof(struct pattern_tracker_req_info_t)*(shrunkSize+1));
	ret->aggPtSize = malloc(sizeof(int)*(shrunkSize+1));
	if((ret->time_series == NULL) || (ret->aggPtSize == NULL))
	{
		agios_print("PANIC! Could not allocate memory for DTW\n");
		free_access_pattern_t(&ret);
		return NULL;
	}
	
	if((shrunkSize > ts->reqnb) || (shrunkSize <= 0))
	{
		agios_print("PANIC! Trying to shrink time series of size %d with non-sensical size %d\n", ts->reqnb, shrunkSize);
		free_access_pattern_t(&ret);
		return NULL;
	}
	ret->reqnb=0;
	ret->original_size = ts->reqnb;

	ptToReadFrom=0;
	while(ptToReadFrom < ts->reqnb)
	{
		ptToReadTo = (int)round(reducedPtSize*(ret->reqnb+1))-1; //determine the end of the current range
		//keep track of the sum of all the values being averaged to create a single point
		sum_timestamps = 0;
		sum_offsets = 0;
		//sum all values over the range
		for(pt = ptToReadFrom; pt <= ptToReadTo; pt++)
		{
			sum_offsets += ts->time_series[pt].offset;
			sum_timestamps += ts->time_series[pt].timestamp;
		}
		//determine the average of the range and add to the new time series
		ret->aggPtSize[ret->reqnb] = ptToReadTo - ptToReadFrom+1;
		ret->time_series[ret->reqnb].offset = sum_offsets/ret->aggPtSize[ret->reqnb];
		ret->time_series[ret->reqnb].timestamp = sum_timestamps/ret->aggPtSize[ret->reqnb];
		ret->reqnb++;
		//next window of points to average starts where the last window ended
		ptToReadFrom = ptToReadTo + 1;
	}
	
	return ret;
}

//=========================================================================================================================================
//functions to manage the search window, that is used to extrapolate results obtained for shrunk versions of the access patterns to the full access patterns. 
//originally a search window is created to each recursive call, but we'll keep a single one (trying to make it large enough) to avoid that
//moreover, we'll also keep a static iterator, which is used to iterate over the window while modifying it 
int SearchWindow_sizeI = -1;
//int SearchWindow_sizeJ = -1;
struct search_window_t *SearchWindow=NULL;
struct search_window_t *SearchWindowIterator=NULL;

inline struct search_window_t * new_search_window(int tsIsize, int tsJsize)
{
	int i;

	if(tsIsize >= SearchWindow_sizeI) //our current search window is not large enough
	{
		//free the current search window (and the iterator)
		if(SearchWindow)
		{
			if(SearchWindow->minValues)
				free(SearchWindow->minValues);
			if(SearchWindow->maxValues)
				free(SearchWindow->maxValues);
		}
		if(SearchWindowIterator)
		{
			if(SearchWindowIterator->minValues)
				free(SearchWindowIterator->minValues);
			if(SearchWindowIterator->maxValues)
				free(SearchWindowIterator->maxValues);
		}
		//allocate a new one
		if(!SearchWindow)
		{
			SearchWindow =malloc(sizeof(struct search_window_t)); 
			if(!SearchWindow)
			{
				agios_print("PANIC! Could not allocate memory for DTW\n");
				return NULL;
			}
		}
		if(!SearchWindowIterator)
		{
			SearchWindowIterator =malloc(sizeof(struct search_window_t)); 
			if(!SearchWindowIterator)
			{
				agios_print("PANIC! Could not allocate memory for DTW\n");
				return NULL;
			}
		}
		SearchWindow->minValues = malloc(sizeof(int)*(tsIsize+1));
		SearchWindow->maxValues = malloc(sizeof(int)*(tsIsize+1)); 
		SearchWindowIterator->minValues = malloc(sizeof(int)*(tsIsize+1));
		SearchWindowIterator->maxValues = malloc(sizeof(int)*(tsIsize+1)); 
		if((!SearchWindow->minValues) | (!SearchWindow->maxValues) | (!SearchWindowIterator->minValues) | (!SearchWindowIterator->maxValues))
		{
			agios_print("PANIC! Could not allocate memory for DTW\n");
			return NULL;
		}
		SearchWindow_sizeI = tsIsize + 1;
	}
	//reset the search window
	for(i=0; i< tsIsize; i++)
		SearchWindow->minValues[i]=-1;
	SearchWindow->maxJ_ = tsJsize-1;
	SearchWindow->size = 0;
	SearchWindow->modCount=0;
	return SearchWindow;
}

inline void SearchWindow_markVisited(struct search_window_t *window, int col, int row)
{
	if(window->minValues[col] < 0) //first value entered in the column
	{
		window->minValues[col] = row;
		window->maxValues[col] = row;
		window->size++;
		window->modCount++; //structure has been changed
	}
	else if (window->minValues[col] > row) //minimum range in the column is expanded
	{
		window->size += window->minValues[col]-row;
		window->minValues[col] = row;
		window->modCount++; //structure has been changed
	}
	else if (window->maxValues[col] < row) //maximum range in the column is expanded
	{
		window->size += row - window->maxValues[col];
		window->maxValues[col] = row;
		window->modCount++;
	}
}

//obtain a new search window structure from an existing one (to iterate over it, we'll use the static structure we've created for that) 
inline struct search_window_t *SearchWindow_obtain(struct search_window_t *window)
{
	//we'll assume we already have it correctly allocated, as we did it when allocating the original search window we're copying
	int currentI, currentJ;
	short int hasMoreElements;
	
	SearchWindowIterator->size=0; //reset it
	if(window->size > 0)
		hasMoreElements = 1;
	else
		hasMoreElements=0;
	currentI = 0;	
	currentJ = 0;

	while(hasMoreElements == 1)
	{
		SearchWindowIterator->minValues[SearchWindowIterator->size] = currentI;
		SearchWindowIterator->maxValues[SearchWindowIterator->size] = currentJ;
		SearchWindowIterator->size++;
		if(++currentJ > window->maxValues[currentI])
		{	
			if(++currentI <= (window->size - 1))
				currentJ = window->minValues[currentI];
			else
				hasMoreElements=0;
		}
	}
	return SearchWindowIterator;
}
//=========================================================================================================================================

void SearchWindow_expandSearchWindow(struct search_window_t *window, int radius)
{
	unsigned int cell;
	int targetCol, targetRow, cellsPastEdge;

	if(radius > 0) //if radius <= 0 then no search is necessary, use the current search window
	{
		//add all cells in the current window to an array, iterating through the window and expanding the window
		//at the same time it is not possible because the window can't be changed during iteration through the cells
		struct search_window_t *windowCells = SearchWindow_obtain(window);
		if(windowCells == NULL)
			return;
		for(cell = 0; cell < windowCells->size; cell++)
		{
			if((windowCells->minValues[cell] != 0) && (windowCells->maxValues[cell] != window->maxJ_)) //move to upper left is possible
			{
				//either expand full search radius or some fraction until edges of matrix are met
				targetCol = windowCells->minValues[cell] - radius;
				targetRow = windowCells->maxValues[cell] + radius;
				if((targetCol >= 0) && (targetRow <= window->maxJ_))
					SearchWindow_markVisited(window, targetCol, targetRow);
				else
				{
					//expand the window only to the edge of the matrix
					cellsPastEdge = fmax(0 - targetCol, targetRow - window->maxJ_);
					SearchWindow_markVisited(window, targetCol + cellsPastEdge, targetRow - cellsPastEdge);
				}
			} 
			if(windowCells->maxValues[cell] != window->maxJ_) //move up if possible
			{
				//either extend full search radius or some fraction until edges of matrix are met
				targetCol = windowCells->minValues[cell];
				targetRow = windowCells->maxValues[cell] + radius;
			
				if(targetRow <= window->maxJ_)
					SearchWindow_markVisited(window, targetCol, targetRow); //radius does not go past the edges of the matrix
				else
				{
					//expand the window only to the edge of the matrix
					cellsPastEdge = targetRow - window->maxJ_;
					SearchWindow_markVisited(window, targetCol, targetRow - cellsPastEdge);
				}
			}
			if((windowCells->minValues[cell] != (window->size-1)) && (windowCells->maxValues[cell] != window->maxJ_)) //move to upper-right if possible
			{
				//either extend full search radius or some fraction until edges of matrix are met
				targetCol = windowCells->minValues[cell] + radius;
				targetRow = windowCells->maxValues[cell] + radius;
				if ((targetCol <= (window->size - 1)) && (targetRow <= window->maxJ_))
					SearchWindow_markVisited(window,targetCol, targetRow); //radius does not go past the edges of the matrix
				else
				{
					//expand the window only to the edge of the matrix
					cellsPastEdge = fmax(targetCol - (window->size-1),targetRow - window->maxJ_);
					SearchWindow_markVisited(window,targetCol - cellsPastEdge, targetRow - cellsPastEdge);
				}
			}
			if(windowCells->minValues[cell] != 0) // move left if possible
			{
				//either extend full search radius or some fraction until edges of matrix are met
				targetCol = windowCells->minValues[cell] - radius;
				targetRow = windowCells->maxValues[cell];
				if(targetCol >= 0)
					SearchWindow_markVisited(window,targetCol, targetRow);
				else
				{
					cellsPastEdge = 0 - targetCol;
					SearchWindow_markVisited(window,targetCol + cellsPastEdge, targetRow);
				}
			}
			if(windowCells->minValues[cell] != (window->size-1)) //move right if possible
			{
				targetCol = windowCells->minValues[cell]+radius;
				targetRow = windowCells->maxValues[cell];
				if(targetCol <= (window->size -1))
					SearchWindow_markVisited(window, targetCol, targetRow);
				else
				{
					cellsPastEdge = targetCol - (window->size-1);
					SearchWindow_markVisited(window, targetCol - cellsPastEdge, targetRow);
				}
			}
			if((windowCells->minValues[cell] != 0) && (windowCells->maxValues[cell] != 0)) //move to lower-left is possible
			{
				targetCol = windowCells->minValues[cell]-radius;
				targetRow = windowCells->maxValues[cell]-radius;
				if((targetCol >= 0) && (targetRow >= 0))
					SearchWindow_markVisited(window, targetCol, targetRow);
				else
				{
					cellsPastEdge = fmin(0 - targetCol, 0 - targetRow); //(fran) modified from the original version (it was max, changed to min) because it seemed wrong
					SearchWindow_markVisited(window, targetCol + cellsPastEdge, targetRow + cellsPastEdge);
				}
			} 
			if (windowCells->maxValues[cell] != 0) //move down if possible
			{
				targetCol = windowCells->minValues[cell];
				targetRow = windowCells->maxValues[cell]-radius;
				if(targetRow >= 0)
					SearchWindow_markVisited(window, targetCol, targetRow);
				else
				{
					cellsPastEdge = 0 - targetRow;
					SearchWindow_markVisited(window, targetCol, targetRow + cellsPastEdge);
				}
			}
			if((windowCells->minValues[cell] != (window->size-1)) && (windowCells->maxValues[cell] != 0)) //move to lower-right if possible
			{
				targetCol = windowCells->minValues[cell] + radius;
				targetRow = windowCells->maxValues[cell] - radius;
				if((targetCol <= (window->size-1)) && (targetRow >= 0))
					SearchWindow_markVisited(window, targetCol, targetRow);
				else
				{
					cellsPastEdge = fmax(targetCol - (window->size-1), 0 - targetRow);
					SearchWindow_markVisited(window, targetCol - cellsPastEdge, targetRow+cellsPastEdge);
				}
			}
		}//end for
	} //end if
}

inline void SearchWindow_expandWindow(struct search_window_t *window, int radius)
{
	if(radius > 0)
	{
		SearchWindow_expandSearchWindow(window, 1);
		SearchWindow_expandSearchWindow(window, radius-1);
	}
}

struct search_window_t *SearchWindow_ExpandedResWindow(struct access_pattern_t *tsI, struct access_pattern_t *tsJ, struct access_pattern_t *shrunkI, struct access_pattern_t *shrunkJ, struct warp_path_t *path, int searchRadius)
{
	int w, warpedI, warpedJ, blockIsize, blockJsize, x;
	//variables to keep track of the current location of the higher resolution projected path
	int currentI = path->tsIindexes[0];
	int currentJ = path->tsJindexes[0];
	//variables to keep track of the last part of the low-resolution warp path that was evaluated (to determine direction
	int lastWarpedI = INT_MAX;
	int lastWarpedJ = INT_MAX;
	struct search_window_t *ret = new_search_window(tsI->reqnb, tsJ->reqnb);
	if(ret == NULL)
	{
		return NULL;
	}
	//for each part of the low-resolution warp path, project that path to the higher resolution by filling in the path's corresponding cells at the higher resolution
	for(w = 0; w < path->index; w++)
	{
		warpedI = path->tsIindexes[w];
		warpedJ = path->tsJindexes[w];
		blockIsize = shrunkI->aggPtSize[warpedI];
		blockJsize = shrunkJ->aggPtSize[warpedJ];
		//if the path moved up or diagonally, then the next cell's values on the J axis will be larger
		if(warpedJ > lastWarpedJ)
			currentJ += shrunkJ->aggPtSize[lastWarpedJ];	
		if(warpedI > lastWarpedI)
			currentI += shrunkI->aggPtSize[lastWarpedI];
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
		//fill in the cells that are created by a projection from the crell in the low-resolution warp path to a higher resolution
		for(x = 0; x < blockIsize; x++)
		{
			SearchWindow_markVisited(ret, currentI+x, currentJ);
			SearchWindow_markVisited(ret, currentI+x, currentJ + blockJsize-1);
		}
		//record the last position in the time warp path so the direction of the path can be determined when the next position of the path is evaluated
		lastWarpedI = warpedI;
		lastWarpedJ = warpedJ;
	}
	//expand the size of the projected warp path by the specified width
	SearchWindow_expandWindow(ret, searchRadius);

	return ret;
}

struct time_warp_info_t *FastDTW_fastDTW(struct access_pattern_t *tsI, struct access_pattern_t *tsJ, int searchRadius)
{
	int minTSsize;
	struct search_window_t *window;
	struct access_pattern_t *shrunkI, *shrunkJ;
	struct time_warp_info_t *ret;

	if(searchRadius < 0)
		searchRadius = 0;

	minTSsize = searchRadius+2;
	if((tsI->reqnb <= minTSsize) || (tsJ->reqnb <= minTSsize))
	{
		//perform full DTW
		return DTW_getWarpInfoBetween(tsI, tsJ); 
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
			window = SearchWindow_ExpandedResWindow(tsI, tsJ, shrunkI, shrunkJ, &ret->path, searchRadius);
			if(window)
			{
				//find the optimal path through this search window constraint
				ret = DTW_getWarpInfoBetween_withWindow(tsI, tsJ, window);
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

struct time_warp_info_t *FastDTW_getWarpInfoBetween(struct access_pattern_t *tsI, struct access_pattern_t *tsJ, int searchRadius)
{
	//before calling the fast dtw function, well allocate some structures using the largest size possible for this calculation, so we won't have to reallocate logN times during it
	if((tsI->reqnb >= tsJ->reqnb) & (tsI->reqnb > max_window_size))
	{
		max_window_size = tsI->reqnb;
		allocate_memory_resident_matrix_t(tsI->reqnb);
	}
	else
	{
		max_window_size = tsJ->reqnb;
		allocate_memory_resident_matrix_t(tsJ->reqnb);
	}
	Check_TimeWarpInfo_allocation(tsI->reqnb, tsJ->reqnb);
	new_search_window(tsI->reqnb, tsJ->reqnb);
		
	return FastDTW_fastDTW(tsI, tsJ, searchRadius);
}

// function called from outside of this module before finishing the execution, to clean up all allocated data structures
inline void DTW_cleanup()
{
	//clean the memory_resident_matrix_t
	if(MRmatrix)
	{
		if(MRmatrix->cellValues)
			free(MRmatrix->cellValues);
		if(MRmatrix->colOffsets)
			free(MRmatrix->colOffsets);
		free(MRmatrix);
	}
	//clean the time_warp_info_t
	if(TWinfo)
	{
		if(TWinfo->path.tsIindexes)
			free(TWinfo->path.tsIindexes);
		if(TWinfo->path.tsJindexes)
			free(TWinfo->path.tsJindexes);
		free(TWinfo);
	}
	//free the cost matrix
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
	//free search windows
	if(SearchWindow)
	{
		if(SearchWindow->minValues)
			free(SearchWindow->minValues);
		if(SearchWindow->maxValues)
			free(SearchWindow->maxValues);
		free(SearchWindow);
	}
	if(SearchWindowIterator)
	{
		if(SearchWindowIterator->minValues)
			free(SearchWindowIterator->minValues);
		if(SearchWindowIterator->maxValues)
			free(SearchWindowIterator->maxValues);
		free(SearchWindowIterator);
	}

	
}

// This is the function called from outside to compare two access patterns A and B. It will return an unsigned long long int that is a score. The higher the score, the higher the distance between the patterns (the more different they are). It will have to be later adapted to a %
unsigned long long int FastDTW(struct access_pattern_t *A, struct access_pattern_t *B)
{
	unsigned long long int ret;
	struct time_warp_info_t *info = FastDTW_getWarpInfoBetween(A, B, 1);
	if(info)
	{
		ret = info->distance;
	}
	else
	{
		agios_print("PANIC! Could not apply fast DTW");
		ret = ULLONG_MAX;
	}
	return ret;
}
