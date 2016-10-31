#include <math.h>

#include "DTW.h"


struct time_warp_info_t * DTW_getWarpInfoBetween(struct access_pattern_t *tsI, struct access_pattern_t *tsJ)
{
	return DTW_DynamicTimeWarp(tsI, tsJ);
} 

//starts a new empty path and allocates memory for the provided maximum size
inline void Initialize_TimeWarp_Path(struct warp_path_t *new, unsigned long int size)
{
	new->tsIindexes = malloc(size);
	new->tsJindexes = malloc(size);
	//TODO check for malloc error
	new->index = 0;
}
//adds a point (a,b) to the beginning of the path
inline void Add_to_TimeWarp_Path(struct warp_path_t *path, int a, int b)
{//TODO should we check for errors if we try adding more stuff than it can fit?
	int i;
	path->index++;
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

struct time_warp_info_t * DTW_DynamicTimeWarp(struct access_pattern_t *tsI, struct access_pattern_t *tsJ)
{
	int i,j;
	struct time_warp_info_t *ret = malloc(sizeof(time_warp_info_t));

	//allocate a cost matrix
	double **costMatrix;
	costMatrix = (double **)malloc(sizeof(double *)*(tsI->reqnb+1));
	for(i = 0; i < tsI->reqnb; i++)
	{
		costMatrix[i] = (double *)malloc(sizeof(double)*(tsJ->reqnb+1));
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
	Initialize_TimeWarp_Path(&ret->path, sizeof(int)*(tsI->reqnb+tsJ->reqnb-1));
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
		return NULL; //TODO Notify error?

	ret = malloc(sizeof(struct access_pattern_t));
	ret->time_series = malloc(sizeof(struct pattern_tracker_req_info_t)*(shrunkSize+1));
	ret->aggPtSize = malloc(sizeof(int)*(shrunkSize+1));
	//TODO check malloc error
	//if((shrunkSize > ts->reqnb) || (shrunkSize <= 0))
		//TODO error
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
	ret->minValues = malloc(sizeof(int)*(tsIsize+1));
	for(i=0; i< tsIsize; i++)
		ret->minValues[i]=-1;
	ret->maxValues = malloc(sizeof(int)*(tsIsize+1));
	//TODO check malloc error
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

inline struct search_window_t *SearchWindow_obtain(struct search_window_t *window)
{
	int currentI, currentJ;
	short int hasMoreElements;
	struct search_window_t *new = malloc(sizeof(struct search_window_t));
	new->minValues = malloc(sizeof(int)*(window->size+1));
	new->maxVales = malloc(sizeof(int)*(window->size+1));
	new->size=0;
	//TODO check malloc error

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
			if(window->maxValues[cell] != window->maxJ_) //move up if possible
			{
			//HERE
		}
			


	//TODO continue here	
}

inline void SearchWindow_expandWindow(struct search_window_t *window, int radius)
{
	if(radius > 0)
	{
		SearchWindow_expandSearchWindow(window, 1);
		SearchWindow_expandSearchWindow(window, radius-1);
	}
}

struct search_window_t *ExpandedResWindow(struct access_pattern_t *tsI, struct access_pattern_t *tsJ, struct access_pattern_t *shrunkI, struct access_pattern_t *shrunkJ, struct warp_path_t *path, int searchRadius)
{
	int w, warpedI, warpedJ, blockIsize, blockJsize, x;
	//variables to keep track of the current location of the higher resolution projected path
	int currentI = path->tsIindexes[0];
	int currentJ = path->tsJindexes[0];
	//variables to keep track of the last part of the low-resolution warp path that was evaluated (to determine direction
	int lastWarpedI = INT_MAX;
	int lastWarpedJ = INT_MAX;
	struct search_window_t *ret = new_search_window(tsI->reqnb, tsJ->reqnb);
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
			markVisited(currentI+x, currentJ);
			markVisited(currentI+x, currentJ + blockJSize-1);
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

		//determine the search window that constrains the area of the cost matrix that will be evaluated based on the warp path found at the previous resolution (smaller time series)
		struct time_warp_info_t *tmp = fastDTW(shrunkI, shrunkJ, searchRadius);
		
	//TODO continue
}

struct time_warp_info_t *FastDTW_getWarpInfoBetween(struct access_pattern_t *tsI, struct access_pattern_t *tsJ, int searchRadius)
{
	return FastDTW_fastDTW(tsI, tsJ, searchRadius);
}

double FastDTW(struct access_pattern_t *A, struct access_pattern_t *B)
{
	struct time_warp_info_t *info = FastDTW_getWarpInfoBetween(A, B, 1);
	double ret = info->distance;
	free(info);
	return ret;
}
