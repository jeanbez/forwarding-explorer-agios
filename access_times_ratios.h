#ifndef _ACCESS_TIMES_RATIOS_H_
#define _ACCESS_TIMES_RATIOS_H_

static float writing_ratios[][2] = { { 8192, 11.81 },
				{ 16384, 8.78 },
				{ 24576, 8.99 },
				{ 32768, 6.00 },
				{ 40960, 8.38 },
				{ 49152, 7.37 },
				{ 57344, 6.51 },
				{ 65536, 2.85 } };

static float reading_ratios[][2] = { { 8192, 36.88 },
				{ 16384, 31.71 },
				{ 24576, 25.17 },
				{ 32768, 21.81 },
				{ 40960, 18.93 },
				{ 49152, 16.85 },
				{ 57344, 15.07 },
				{ 65536, 11.80 } };

#define NUMBER_OF_RATIOS 8

#define RATIOS_STEP 8

#endif
