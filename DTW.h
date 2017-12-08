#ifndef _DTW_H_
#define _DTW_H_

#include "mylist.h"
#include "pattern_tracker.h"

struct DTW_point {
	int i;
	int j;
};

struct time_warp_info_t {
	//distance
	long int distance;
	//path that results in this distance
	struct DTW_point *points;
	int index;
	int max;
};

struct DTW_range {
	int min;
	int max;
};

struct search_window_t {
	struct DTW_range *ranges;
	int maxJ;
	int maxI;
	int size;
};

struct search_window_iterator_t {
	struct DTW_point *points;
	int size;
}; 

struct constrainedDTW_cost_t {
	long int *cellValues;
	int cellValues_size;
	int *colOffsets;
	int colOffsets_size;
};

long int DTW(struct access_pattern_t *tsI, struct access_pattern_t *tsJ);

long int FastDTW(struct access_pattern_t *A, struct access_pattern_t *B);


void DTW_cleanup();
#endif
