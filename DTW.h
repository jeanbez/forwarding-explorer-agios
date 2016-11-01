#ifndef _DTW_H_
#define _DTW_H_

#include "mylist.h"
#include "pattern_tracker.h"


struct warp_path_t {
	int * tsIindexes;
	int * tsJindexes;
	int index;
};

struct time_warp_info_t {
	double distance;
	struct warp_path_t path;
};
struct time_series_t {
	unsigned long int *time;
	long long int *offset;
	int size;
};

struct search_window_t {
	int *minValues;
	int *maxValues;
	int maxJ_;
	int size;
	int modCount;
};

struct memory_resident_matrix_t {
	double *cellValues;
	int cellValues_size;
	int *colOffsets;
	int colOffsets_size;
};

double FastDTW(struct access_pattern_t *A, struct access_pattern_t *B);

#endif
