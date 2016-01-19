#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <agios.h>


#define REQ_TYPE 0
#define MAX_SLEEP_TIME 1000000 //in ns

static int processed_reqnb=0;
static pthread_mutex_t processed_reqnb_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t processed_reqnb_cond=PTHREAD_COND_INITIALIZER;

static int generated_reqnb;
static int reqnb_perthread;
static int thread_nb;
static int req_size;
static int time_between;
static unsigned long int *req_offset;

struct client clnt;

static pthread_t *threads;
static pthread_barrier_t test_start;
static pthread_t *processing_threads;
static int processing_threads_index;

struct req_id_t
{
	int reqid;
	int threadid;
};

void inc_processed_reqnb()
{
	pthread_mutex_lock(&processed_reqnb_mutex);
	processed_reqnb++;
	if(processed_reqnb >= generated_reqnb)
		pthread_cond_signal(&processed_reqnb_cond);
	pthread_mutex_unlock(&processed_reqnb_mutex);
}

void *process_request_thr(void *arg)
{
	struct req_id_t *req = (struct req_id_t *)arg;
	char *filename = malloc(sizeof(char)*100);

	int sleep_time = rand() % MAX_SLEEP_TIME;
	struct timespec sleep_time_tsp;

	printf("thread created to process request starting ");
	printf("our request is %d\n", req->reqid);

	sleep_time_tsp.tv_sec = (unsigned int) sleep_time / 1000000000L;
	sleep_time_tsp.tv_nsec = (unsigned int) sleep_time % 1000000000L;
	nanosleep(&sleep_time_tsp, NULL);

	snprintf(filename, sizeof(char)*99, "arquivo.%d.out", req->threadid);
	agios_release_request(filename, REQ_TYPE, req_size, req_offset[req->reqid]);

	inc_processed_reqnb();
	free(req);	
}

void test_process(void * req_id)
{
	struct req_id_t *req = (struct req_id_t *)req_id;
	int ret;

	ret = pthread_create(&processing_threads[processing_threads_index], NULL, process_request_thr, req);		
	processing_threads_index++;
	if(ret != 0)
	{
		printf("PANIC! Unable to create thread to process request!\n");
	}
}

/*thread that will generate tons of requests to AGIOS*/
void *test_thr(void *arg)
{
	char *filename = malloc(sizeof(char)*100);
	unsigned long int offset = 0;
	int i;
	struct timespec timeout;
	struct req_id_t *req_id;
	int threadid = (int)pthread_self();

	printf("Thread %d starting its execution\n", threadid);

	sprintf(filename, "arquivo.%d.out", threadid);
//	printf("starting generation of requests to file %s\n", filename);

	/*wait for the start signal*/
	pthread_barrier_wait(&test_start);

	for(i=0; i<reqnb_perthread; i++)
	{
		req_id = malloc(sizeof(struct req_id_t));
		req_id->reqid = i;
		req_id->threadid = threadid;
		/*generate a request*/
		agios_add_request(filename, REQ_TYPE, offset, req_size, (void *) req_id, &clnt);
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

	srand(1512);
	/*get arguments*/
	if(argc < 6)
	{
		printf("Usage: ./%s <number of threads> <number of requests per thread> <requests' size in bytes> <time between requests in ns> <stats file name>\n", argv[0]);
		exit(1);
	}
	thread_nb=atoi(argv[1]);
	reqnb_perthread = atoi(argv[2]);
	generated_reqnb = reqnb_perthread * thread_nb;
	req_size = atoi(argv[3]);
	time_between = atoi(argv[4]);
	req_offset = malloc(sizeof(unsigned long int)*(reqnb_perthread+1));
	req_offset[0] = 0;
	for (i=1; i < reqnb_perthread; i++)
		req_offset[i] = req_offset[i-1]+req_size;

	/*start AGIOS*/
	clnt.process_requests = NULL;
	clnt.process_request = test_process;
	if(agios_init(&clnt, "/tmp/agios.conf") != 0)
	{
		printf("PANIC! Could not initialize AGIOS!\n");
		exit(1);
	}

	/*generate the threads*/
	printf("Generating %d threads. Each one of them will issue %d requests of %d bytes every %dns\n", thread_nb, reqnb_perthread, req_size, time_between);

	pthread_barrier_init(&test_start, NULL, thread_nb+1);
	threads = malloc(sizeof(pthread_t)*(thread_nb+1));
	processing_threads=malloc(sizeof(pthread_t)*((thread_nb*reqnb_perthread)+1));
	processing_threads_index=0;
	for(i=0; i< thread_nb; i++)
	{
		ret = pthread_create(&(threads[i]), NULL, test_thr, NULL);		
		if(ret != 0)
		{
			printf("PANIC! Unable to create thread %d!\n", i);
			free(threads);
			free(processing_threads);
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

	free(processing_threads);
	free(threads);

	return 0;
}
