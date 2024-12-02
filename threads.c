#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>

#define MODVAL 1000000
#define THREAD_CT 8
#define ARR_SIZE 500000
#define SEED 5
#define MAX_RAND 100

static int total = 0;
static pthread_mutex_t mutex;

struct timeval tv_delta(struct timeval start, struct timeval end) {
	struct timeval delta = end;

	delta.tv_sec -= start.tv_sec;
	delta.tv_usec -= start.tv_usec;
	if (delta.tv_usec < 0) {
		delta.tv_usec += 1000000;
		delta.tv_sec--;	
	}

	return delta;
}

typedef struct {
	int *arr, *arr_end;
} SumArgs;

void *sum(void *args) {
	SumArgs *arg = (SumArgs*) args;	
	
	while (arg->arr < arg->arr_end) {
		pthread_mutex_lock(&mutex);	
		if (arg->arr < arg->arr_end) {
			total = (total + *(arg->arr)) % MODVAL;	
			arg->arr++;
		}
		pthread_mutex_unlock(&mutex);
	}

	return NULL;
}

int run_sum(int *arr, int arr_size, int thread_ct) {
	int i;
	pthread_t *tids = calloc(arr_size, sizeof(int));
	int *arr_end = arr + arr_size;
	SumArgs args;

	args.arr = arr;
	args.arr_end = arr_end;
	
	pthread_mutex_init(&mutex, NULL);

	for (i = 0; i < thread_ct; i++) {
		pthread_create(&tids[i], NULL, sum, &args);	
	}	

	for (i = 0; i < thread_ct; i++) {
		pthread_join(tids[i], NULL);	
	}
	
	pthread_mutex_destroy(&mutex);	
	free(tids);

	return total;
}

int main(void) {
	/* Will need to get these values from command line args */
	int thread_ct = THREAD_CT;	
	int i, arr_size = ARR_SIZE;
	int seed = SEED;
	int result = -1, test_sum = 0;
	int *arr; 
	int task = 2;
	char print_results = 'Y';
	struct rusage start_ru, end_ru;
	struct timeval start_wall, end_wall;
	struct timeval diff_ru_utime, diff_wall, diff_ru_stime;
	
	fflush(stdout);
	arr = calloc(arr_size, sizeof(int));

	/* Filling array with random integers */
	srand(seed);
	for (i = 0; i < arr_size; i++) {
		arr[i] = rand() % MAX_RAND;	
		/* The true sum to test threaded sum */
		test_sum = (test_sum + arr[i]) % MODVAL;
	}

	if (task == 2) {
		getrusage(RUSAGE_SELF, &start_ru);
		gettimeofday(&start_wall, NULL);

		result = run_sum(arr, arr_size, thread_ct); 	

		gettimeofday(&end_wall, NULL);
		getrusage(RUSAGE_SELF, &end_ru);

		diff_ru_utime = tv_delta(start_ru.ru_utime, end_ru.ru_utime);
		diff_ru_stime = tv_delta(start_ru.ru_stime, end_ru.ru_stime);
		diff_wall = tv_delta(start_wall, end_wall);
		
		if (print_results == 'Y') {
			printf("Results: %d\n", result);	
			printf("Test Sum: %d\n", test_sum);
			printf("User time: %ld.%06ld\n", diff_ru_utime.tv_sec, diff_ru_utime.tv_usec);
			printf("System time: %ld.%06ld\n", diff_ru_stime.tv_sec, diff_ru_stime.tv_usec);
			printf("Wall time: %ld.%06ld\n", diff_wall.tv_sec, diff_wall.tv_usec);
		}
	}	
	
	free(arr);

	return 0;
}

