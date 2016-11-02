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

/* File:	DTW.h
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
#ifndef _DTW_H_
#define _DTW_H_

#include "mylist.h"
#include "pattern_tracker.h"


struct warp_path_t {
	int * tsIindexes;
	int * tsJindexes;
	int index;
	int max;
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
