
#ifndef _ESTIMATE_ACCESS_TIMES_H_
#define _ESTIMATE_ACCESS_TIMES_H_

void read_access_times_functions(char *filename);
long long int get_access_time(long int size, int operation);
float get_access_ratio(long int size, int operation);
#endif
