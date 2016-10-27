#include <math.h>

#include "DTW.h"


struct time_warp_info_t * DTW_getWarpInfoBetween(struct access_pattern_t *tsI, struct access_pattern_t *tsJ)
{
	return DTW_DynamicTimeWarp(tsI, tsJ);
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
	ret->path.tsIindexes = malloc(sizeof(int)*tsI->reqnb+tsJ->reqnb-1);
	ret->path.tsJindexes = malloc(sizeof(int)*tsI->reqnb+tsJ->reqnb-1);
	//TODO check for malloc errors
	i = tsI->reqnb-1;
	j = tsJ->reqnb-1;
	ret->path.tsIindexes[0] = i;
	ret->path.tsJindexes[0] = j;
//	TODO CONTINUE HERE
	

	//TODO if we want the path, we need to continue the implementation
	for(i=0; i< tsI->reqnb; i++)
		free(costMatrix[i]);
	free(costMatrix);
	return ret;
}

inline double DTW_euclideanDist(struct access_pattern_t *tsI, int indexI, struct access_pattern_t *tsJ, int indexJ)
{
	return pow(sqrt(pow(tsI->time_series[indexI].offset - tsJ->time_series[indexJ].offset, 2.0)), 2.0);
}

struct time_series_t

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
		double resolutionFactor = 2.0;
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
