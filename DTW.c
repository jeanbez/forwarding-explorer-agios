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
 * Translated to C and adapted by Francieli Zanon Boito
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
 *
 *		inspired in Adrien Lebre's aIOLi framework implementation
 *	
 *		This program is distributed in the hope that it will be useful,
 * 		but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <math.h>

#include "common_functions.h"
#include "DTW.h"


inline struct time_warp_info_t * DTW_getWarpInfoBetween(struct access_pattern_t *tsI, struct access_pattern_t *tsJ)
{
	return DTW_DynamicTimeWarp(tsI, tsJ);
} 
inline struct time_warp_info_t *DTW_getWarpInfoBetween_withWindow(struct access_pattern_t *tsI, struct access_pattern_t *tsJ, struct search_window_t *window)
{
	return DTW_constrainedTimeWarp(tsI, tsJ, window);
}

//allocates and initializes a new memory_resident_matrix_t structure, used by constrained DTW. Returns NULL on error
inline struct memory_resident_matrix_t *get_new_memory_resident_matrix_t(struct seach_window_t *window)
{
	int i;
	int currentOffset=0;

	struct memory_resident_matrix_t *ret = malloc(sizeof(struct memory_resident_matrix_t));
	if(ret == NULL)
	{
		agios_print("PANIC! Could not allocate memory for DTW\n");
		return NULL;
	}
	ret->cellValues = malloc(sizeof(double)*window->size);
	if(ret->cellValues == NULL)
	{
		agios_print("PANIC! Could not allocate memory for DTW\n");
		return NULL;
	}
	ret->cellValues_size=0;
	ret->colOffsets = malloc(sizeof(int)*window->size);
	if(ret->colOffsets == NULL)
	{
		agios_print("PANIC! Could not allocate memory for DTW\n");
		return NULL;
	}
	ret->colOffsets_size=0;

	for(i = 0; i < window->size; i++)
	{
		ret->colOffsets[i] = currentOffset;
		ret->colOffsets_size++;
		currentOffset += window->maxValues[i] - window->minValues[i] + 1;
	}
	return ret;	
}
inline void add_to_memory_resident_matrix_t(struct memory_resident_matrix_t *matrix, struct search_window_t *window, int col, int row, double value)
{
	if((row < window->minValues[col]) || (row > window->maxValues[col]))
	{
		agios_print("PANIC! Trying to add a cell to memory resident matrix which is out of the limits given by the window.\n");
		//now what?
	}
	else
		matrix->cellValues[matrix->colOffsets[col]+row-window->minValues[col]] = value;
}
inline double get_from_memory_resident_matrix_t(struct memory_resident_matrix_t *matrix, struct search_window_t *window, int col, int row)
{
	if((row < window->minValues[col]) || (row > window->maxValues[col]))
		return DBL_MAX;
	else
		return matrix->cellValues[matrix->colOffsets[col]+row - window->minValues[col]];
}
//frees previously allocated memory_resident_matrix_t structure, including its internal lists
inline void free_memory_resident_matrix_t(struct memory_resident_matrix_t **matrix)
{
	if(*matrix)	
	{
		if((*matrix)->cellValues)
			free((*matrix)->cellValues);
		if((*matrix)->colOffsets)
			free((*matrix)->colOffsets);
		free(*matrix);
	}
}
struct time_warp_info_t *DTW_constrainedTimeWarp(struct access_pattern_t *tsI, struct access_pattern_t *tsJ, struct search_window_t *window)
{
	unsigned int colu, i, j;
	double diagCost, leftCost, downCost;
	struct time_warp_info_t *ret = malloc(sizeof(struct time_warp_info)); 
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
		free_time_warp_info_t(&ret);
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
			add_to_memory_resident_matrix_t(costMatrix, window, i, j, DTW_euclideanDist(tsI->time_series[i].offset, tsJ->time_series[0]) + get_from_memory_resident_matrix_t(costMatrix, window, i-1,j));
		else //not first column or first row		
			add_to_memory_resident_matrix_t(costMatrix, window, i, j, fmin(get_from_memory_resident_matrix_t(costMatrix, window, i-1, j), fmin(get_from_memory_resident_matrix_t(costMatrix, window, i-1,j-1), get_from_memory_resident_matrix_t(costMatrix, window, i,j-1))) + DTW_euclideanDist(tsI->time_series[i].offset, tsJ->time_series[j].offset));
	} // end for
	free_search_window_t(&matrixIterator);
	//minimum cost is at (max i, max j)
	ret->distance = get_from_memory_resident_matrix_t(costMatrix, window, maxI, maxJ);

	//find the warp path by searching the matrix from the solution at (max i, max j) to the beginning at (0,0). at each step move through the matrix 1 step left, down, or diagonal, whichever has the smallest cost. Favor diagonal moves and moves towards the i==j axis to break ties
	Initialize_TimeWarp_Path(&ret->path, (maxI + maxJ - 1));
	i = maxI;
	j = maxJ;
	Add_to_TimeWarp_Path(&ret->path, i, j);
	
	while((i > 0) || (j > 0))
	{
		//find the costs of moving in all three possible directions (left, down, and diagonal (down and left at the same time)
		if((i > 0)&& (j > 0))
			diagCost = get_from_memory_resident_matrix_t(costMatrix, window, i-1, j-1);
		else
			diagCost = DBL_MAX;
		if(i > 0)
			leftCost = get_from_memory_resident_matrix_t(costMatrix, window, i-1,j);
		else
			leftCost = DBL_MAX;
		if(j > 0)
			downCost = get_from_memory_resident_matrix_t(costMatrix, window, i, j-1);
		else
			downCost = DBL_MAX;
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


	//cleanup
	free_memory_resident_matrix_t(&costMatrix);

	return ret;
}

//starts a new empty path and allocates memory for the provided maximum size
inline void Initialize_TimeWarp_Path(struct warp_path_t *new, unsigned long int size)
{
	new->tsIindexes = malloc(sizeof(int)*size);
	new->tsJindexes = malloc(sizeof(int)*size);
	if((new->tsIindexes == NULL) || (new->tsJindexes == NULL))
	{	
		agios_print("PANIC! Cannot allocate memory for DTW.\n");
		//now what?
	}
	new->index = 0;
	new->max = size;
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

struct time_warp_info_t * DTW_DynamicTimeWarp(struct access_pattern_t *tsI, struct access_pattern_t *tsJ)
{
	int i,j;
	struct time_warp_info_t *ret = malloc(sizeof(time_warp_info_t));
	if(ret == NULL)
	{
		agios_print("PANIC! Could not allocate memory for DTW");
		return NULL;
	}

	//allocate a cost matrix
	double **costMatrix;
	costMatrix = (double **)malloc(sizeof(double *)*(tsI->reqnb+1));
	if(costMatrix == NULL)
	{
		agios_print("PANIC! Could not allocate memory for DTW");
		free_time_warp_info_t(&ret);
		return NULL;
	}
	for(i = 0; i < tsI->reqnb; i++)
	{
		costMatrix[i] = (double *)malloc(sizeof(double)*(tsJ->reqnb+1));
		if(costMatrix[i] == NULL)
		{
			int index;
			agios_print("PANIC! Could not allocate memory for DTW");
			free_time_warp_info_t(&ret);
			for(index = 0; index < i; index++)
				free(costMatrix[index]);
			free(costMatrix);
			return NULL;
		}
		for(j=0; j < tsJ->reqnb; j++)
			costMatrix[i][j] = 0;
	}

	//calculate the values for the first column, from the bottom up
	costMatrix[0][0] = DTW_euclideanDist(tsI,0, tsJ,0);
	for(j = 1; j < tsJ->reqnb; j++)
		costMatrix[0][j] = costMatrix[0][j-1] + DTW_euclideanDist(tsI, 0, tsJ, j);
	//now for the rest of the matrix
	for(i = 1; i< tsI->reqnb; i++) 
	{
		//calculate for the bottom row
		costMatrix[i][0] = costMatrix[i-1][0] + DTW_euclideanDist(tsI, i, tsJ, 0);
		//now for the other rows
		for(j = 1; j < tsJ->reqnb; j++)
			costMatrix[i][j] = (fmin(costMatrix[i-1][j], fmin(costMatrix[i-1][j-1], costMatrix[i][j-1]))) + DTW_euclideanDist(tsI, i, tsJ, j);
	}
	//ok, now we have minimum cost
	ret->distance = costMatrix[tsI->reqnb-1][tsJ->reqnb-1];

	//find the warp path by searching the matrix form the solution to at (maxI, maxJ) to the beginning at (0,0). At each step move through the matrix 1 step left, down, or diagonal, whichever has the smallest cost. Favor diagonal moves and moves towards the i==j axis to break ties.
	Initialize_TimeWarp_Path(&ret->path, (tsI->reqnb+tsJ->reqnb-1));
	i = tsI->reqnb-1;
	j = tsJ->reqnb-1;
	Add_to_TimeWarp_Path(&ret->path, i, j);
	while((i > 0) || (j > 0))
	{
		//find the costs of moving in all three possible directions (left, down, and diagonal (down and left at the same time)
		double diagCost, leftCost, downCost;
		
		if(( i > 0) & (j > 0))
			diagCost = costMatrix[i-1][j-1];
		else
			diagCost = DBL_MAX;
		if (i > 0)
			leftCost = costMatrix[i-1][j];
		else
			leftCost = DBL_MAX;
		if(j > 0)
			downCost = costMatrix[i][j-1];
		else
			downCost = DBL_MAX;
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

	//free memory allocated for cost matrix	
	for(i=0; i< tsI->reqnb; i++)
		free(costMatrix[i]);
	free(costMatrix);

	return ret;
}

inline double DTW_euclideanDist(struct access_pattern_t *tsI, int indexI, struct access_pattern_t *tsJ, int indexJ)
{
	return pow(sqrt(pow(tsI->time_series[indexI].offset - tsJ->time_series[indexJ].offset, 2.0)), 2.0);
}

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
	ret->renqb=0;
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

inline struct search_window_t * new_search_window(int tsIsize, int isJsize)
{
	int i;
	struct search_window_t *ret = malloc(sizeof(struct search_window_t));
	if(ret == NULL)
	{
		agios_print("PANIC! Could not allocate memory for DTW\n");
		return NULL;
	}
	ret->minValues = malloc(sizeof(int)*(tsIsize+1));
	ret->maxValues = malloc(sizeof(int)*(tsIsize+1));
	if((ret->minValues == NULL) || (ret->maxValues == NULL))
	{
		agios_print("PANIC! Could not allocate memory for DTW\n");
		free_search_window_t(&ret);
		return NULL;
	}
	for(i=0; i< tsIsize; i++)
		ret->minValues[i]=-1;
	ret->maxJ_ = tsJsize-1;
	ret->size = 0;
	ret->modCount=0;
	return ret;
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

//free an existing search_window_t structure (freeing first its two lists of integers)
inline void free_search_window_t(struct search_window_t **window)
{
	if(*window)
	{
		if((*window)->minValues)
			free((*window)->minValues);
		if((*window)->maxValues)
			free((*window)->maxValues);
		free(*window);
	}
}
//obtain a new search window structure from an existing one (it will allocate a new structure, needs to free it later)
inline struct search_window_t *SearchWindow_obtain(struct search_window_t *window)
{
	int currentI, currentJ;
	short int hasMoreElements;
	struct search_window_t *new = malloc(sizeof(struct search_window_t));
	if(new == NULL)
	{
		agios_print("PANIC! Could not allocate memory for DTW\n");
		return NULL;
	}
	new->minValues = malloc(sizeof(int)*(window->size+1));
	new->maxValues = malloc(sizeof(int)*(window->size+1));
	if((new->minValues == NULL) || (new->maxValues == NULL))
	{
		agios_print("PANIC! Could not allocate memory for DTW\n");
		free_search_window_t(&new);
		return NULL;
	}
	new->size=0;

	if(window->size > 0)
		hasMoreElements = 1;
	else
		hasMoreElements=0;
	currentI = 0;	
	currentJ = 0;

	while(hasMoreElements == 1)
	{
		new->minValues[new->size] = currentI;
		new->maxValues[new->size] = currentJ;
		new->size++;
		if(++currentJ > window->maxValues[currentI])
		{	
			if(++currentI <= (window->size - 1))
				currentJ = window->minValues[currentI];
			else
				hasMoreElements=0;
		}
	}
	return new;
}

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
					searchWindow_markVisited(window, targetCol, targetRow - cellsPastEdge);
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
		free_search_window_t(&windowCells);
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
		for(x = 0; x < blockISize; x++)
		{
			SearchWindow_markVisited(currentI+x, currentJ);
			SearchWindow_markVisited(currentI+x, currentJ + blockJSize-1);
		}
		//record the last position in the time warp path so the direction of the path can be determined when the next position of the path is evaluated
		lastWarpedI = warpedI;
		lastWarpedJ = warpedJ;
	}
	//expand the size of the projected warp path by the specified width
	SearchWindow_expandWindow(ret, searchRadius);

	return ret;
}

//frees a time_warp_info_t structure previously allocated, including the path
inline void free_time_warp_info_t(struct time_warp_info_t **info)
{
	if(*info)
	{
		if((*info)->path.tsIindexes)
			free((*info)->path.tsIindexes);
		if((*info)->path.tsJindexes)
			free((*info)->path.tsJindexes);
		free(*info);
	}
}

struct time_warp_info_t *FastDTW_fastDTW(struct access_pattern_t *tsI, struct access_pattern_t *tsJ, int searchRadius)
{
	int minTSsize;

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
		struct access_pattern_t *shrunkI = shrink_time_series(tsI, (tsI->reqnb / 2));
		struct access_pattern_t *shrunkJ = shrink_time_series(isJ, (tsJ->reqnb / 2));
		if((shrunkI == NULL) || (shrunkJ == NULL))
		{
			agios_print("PANIC! Could not shrink time series from access patterns\n");
			return NULL;
		}

		//determine the search window that constrains the area of the cost matrix that will be evaluated based on the warp path found at the previous resolution (smaller time series)
		struct time_warp_info_t *tmp = FastDTW_fastDTW(shrunkI, shrunkJ, searchRadius);
		if(tmp == NULL)
		{
			free_access_pattern_t(&shrunkI);
			free_access_pattern_t(&shrunkJ);
			return NULL;
		}
		struct search_window_t *window = SearchWindow_ExpandedResWindow(tsI, tsJ, shrunkI, shrunkJ, &tmp->path, searchRadius);
		if(window == NULL)
		{
			free_access_pattern_t(&shrunkI);
			free_access_pattern_t(&shrunkJ);
			free_time_warp_info_t(&tmp);
			return NULL;
		}
		//find the optimal path through this search window constraint
		struct time_warp_info_t *ret = DTW_getWarpInfoBetween_withWindow(tsI, tsJ, window);
		if(ret == NULL)
		{
			free_access_pattern_t(&shrunkI);
			free_access_pattern_t(&shrunkJ);
			free_search_window_t(&window);
			free_time_warp_info_t(&tmp);
			return NULL;
		}
		
		//cleanup
		free_search_window_t(&window);
		free_time_warp_info_t(&tmp);
		free_access_pattern_t(&shrunkI);
		free_access_pattern_t(&shrunkJ);
		return ret;
	}
}

struct time_warp_info_t *FastDTW_getWarpInfoBetween(struct access_pattern_t *tsI, struct access_pattern_t *tsJ, int searchRadius)
{
	return FastDTW_fastDTW(tsI, tsJ, searchRadius);
}

double FastDTW(struct access_pattern_t *A, struct access_pattern_t *B)
{
	double ret;
	struct time_warp_info_t *info = FastDTW_getWarpInfoBetween(A, B, 1);
	if(info)
	{
		ret = info->distance;
		free_time_warp_info_t(info);
	}
	else
	{
		agios_print("PANIC! Could not apply fast DTW");
		ret = DBL_MAX;
	}
	return ret;
}
