#include <pthread.h>
#include  <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <agios.h>
#include <string.h>

struct client clnt;

int day_carry = 0;
int last_hour = 0;
long reqnb=0;
long processed_reqnb=0;
pthread_mutex_t processed_reqnb_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t processed_reqnb_cond=PTHREAD_COND_INITIALIZER;

double calculate_timestamp(int hour, int minute, double seconds)
{
	if(hour < last_hour) // a new day
		day_carry++;
	last_hour = hour;

	return (((day_carry*24) + hour)*60 + minute)*60 + seconds;
}

int main(int argc, char *argv[])
{
	FILE *fd_in;
	char line[1000];
	char *token_handle, *token_type, *token_offset, *token_len, *token, *handle, *token_timestamp;
	int type;
	long offset, len;
	int hour, minute;
	double seconds, timestamp;

	//get arguments: input_file
	// and open file
	if(argc < 2)
		exit(-1);
	else
	{
		fd_in = fopen(argv[1], "r");
		if(!fd_in)
		{
			printf("Could not open file %s!\n", argv[1]);
			perror("fopen");
			exit(-1);
		}
	}	

	//start AGIOS
	clnt.process_requests = NULL;
	clnt.process_request = test_process;
	if(agios_init(&clnt, "tmp/agios.conf", 3) != 0)
	{
		printf("PANIC! Could not initialize AGIOS!\n");
		exit(-1);
	}

	//reads requests from the file and make them to AGIOS
	while(fgets(line, 1000, fd_in))
	{
		//separate request information to give to AGIOS
		//1. by ',' in big parts
		token_timestamp = strtok(line, ",");
		token_handle = strtok(NULL, ",");
		token_type = strtok(NULL, ",");
		token_type = strtok(NULL, ",");
		token_offset = strtok(NULL, ",");
		token_len = strtok(NULL, ",");
		//2 let's get the handle
		token = strtok(token_handle, ": ");
		handle = strtok(NULL, ": ");
		//3 type
		token = strtok(token_type, ": ");
		type = atoi(strtok(NULL, ": "));
		//4 offset
		token = strtok(token_offset, ": ");
		offset = atol(strtok(NULL, ": "));
		//5 len
		token = strtok(token_len, ": ");
		len = atol(strtok(NULL, ": "));
		//6 timestamp
		token = strtok(token_timestamp, "]");
		token_timestamp = strtok(token, "D ");
		token_timestamp = strtok(NULL, "D ");
		hour = atoi(strtok(token_timestamp, ":"));
		minute = atoi(strtok(NULL, ":"));
		seconds = atof(strtok(NULL, ":"));
		timestamp = calculate_timestamp(hour, minute, seconds);

		reqnb++;

	//	printf("%d:%d:%f %f %s %d %ld %ld\n", hour, minute, seconds, timestamp, handle, type, offset, len);
		
		
	}
	fclose(fd_in);

	//wait until we've processed all issued requests
	pthread_mutex_lock(&processed_reqnb_mutex);
	while(processed_reqnb < reqnb)
		pthread_cond_wait(&processed_reqnb_cond, &processed_reqnb_mutex);
	pthread_mutex_unlock(&processed_reqnb_mutex);

	//stop agios and finish
	agios_exit();
}

