#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <agios.h>


#define REQ_TYPE 0

int processed_reqnb=0;
pthread_mutex_t processed_reqnb_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t processed_reqnb_cond=PTHREAD_COND_INITIALIZER;

int generated_reqnb;
int reqnb_perthread;
int thread_nb;
int req_size;
int time_between;

struct client clnt;

static pthread_t *threads;
pthread_barrier_t test_start;

void inc_processed_reqnb()
{
	pthread_mutex_lock(&processed_reqnb_mutex);
	processed_reqnb++;
	if(processed_reqnb >= generated_reqnb)
		pthread_cond_signal(&processed_reqnb_cond);
	pthread_mutex_unlock(&processed_reqnb_mutex);
}

void test_process(int req_id)
{
	inc_processed_reqnb();	
}

/*thread that will generate tons of requests to AGIOS*/
void *test_thr(void *arg)
{
	char *filename = malloc(sizeof(char)*100);
	long long offset = 0;
	int i;
	struct timespec timeout;

	sprintf(filename, "arquivo.%d.out", (int)pthread_self());
//	printf("starting generation of requests to file %s\n", filename);

	/*wait for the start signal*/
	pthread_barrier_wait(&test_start);

	for(i=0; i<reqnb_perthread; i++)
	{
		/*generate a request*/
		agios_add_request(filename, REQ_TYPE, offset, req_size, i, &clnt);
		offset += req_size;
		 
		/*wait a while before generating the next one*/
		timeout.tv_sec = time_between / 1000000000L;
		timeout.tv_nsec = time_between % 1000000000L;
		nanosleep(&timeout, NULL);
	}
}

int main (int argc, char **argv)
{
	int i, ret;
	struct timespec start_time, end_time;
	char **filenames;
	unsigned long long int elapsed;

	/*get arguments*/
	if(argc < 6)
	{
		printf("Usage: ./%s <number of threads> <number of requests per thread> <requests' size in bytes> <time between requests in ns> <stats file name>\n", argv[0]);
		exit(1);
	}
	
	/*start AGIOS*/
	clnt.process_requests = NULL;
	clnt.process_request = test_process;
	if(agios_init(&clnt, "/tmp/agios.conf") != 0)
	{
		printf("PANIC! Could not initialize AGIOS!\n");
		exit(1);
	}

	thread_nb=atoi(argv[1]);
	reqnb_perthread = atoi(argv[2]);
	generated_reqnb = reqnb_perthread * thread_nb;
	req_size = atoi(argv[3]);
	time_between = atoi(argv[4]);

	/*generate the threads*/
	printf("Generating %d threads. Each one of them will issue %d requests of %d bytes every %dns\n", thread_nb, reqnb_perthread, req_size, time_between);

	pthread_barrier_init(&test_start, NULL, thread_nb+1);
	threads = malloc(sizeof(pthread_t)*(thread_nb+1));
	for(i=0; i< thread_nb; i++)
	{
		ret = pthread_create(&(threads[i]), NULL, test_thr, NULL);		
		if(ret != 0)
		{
			printf("PANIC! Unable to create thread %d!\n", i);
			free(threads);
			exit(1);
		}
	}

	/*start timestamp*/
	clock_gettime(CLOCK_MONOTONIC, &start_time);

	/*give the start signal to the threads*/
	pthread_barrier_wait(&test_start);

	/*wait until all requests have been processed*/
	pthread_mutex_lock(&processed_reqnb_mutex);
	while(processed_reqnb < generated_reqnb)
		pthread_cond_wait(&processed_reqnb_cond, &processed_reqnb_mutex);
	pthread_mutex_unlock(&processed_reqnb_mutex);

	/*end timestamp*/
	clock_gettime(CLOCK_MONOTONIC, &end_time);

	/*calculate and print the throughput*/
	elapsed = ((end_time.tv_nsec - start_time.tv_nsec) + ((end_time.tv_sec - start_time.tv_sec)*1000000000L));
	printf("It took %lluns to generate and schedule %d requests. The thoughput was of %f requests/s\n", elapsed, generated_reqnb, ((double) (generated_reqnb) / (double) elapsed)*1000000000L);	
	
	agios_print_stats_file(argv[5]);
	agios_exit();

	return 0;
}
