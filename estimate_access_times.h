
#ifndef _ESTIMATE_ACCESS_TIMES_H_
#define _ESTIMATE_ACCESS_TIMES_H_

int read_access_times_functions(char *filename);
long int get_access_time(long int size, int operation);
double get_access_ratio(long int size, int operation);
void access_times_functions_cleanup(void);

#endif
