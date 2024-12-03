#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#define MODVAL 1000000
#define MAX_RAND 100


static int total = 0;
static pthread_mutex_t mutex;


typedef struct {
    int num_elements; 
    int num_threads; 
    unsigned int seed;
    int task;
    int print_results;
} CommandLineArgs;



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


int parse_arguments(int argc, char *argv[], CommandLineArgs *args) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <num_elements> <num_threads> <seed> <task> <print_results>\n", argv[0]);
        return -1;
    }

    args->num_elements = atoi(argv[1]);
    if (args->num_elements <= 0) {
        fprintf(stderr, "Error: Number of elements must be positive\n");
        return -1;
    }

    args->num_threads = atoi(argv[2]);
    if (args->num_threads <= 0) {
        fprintf(stderr, "Error: Number of threads must be positive\n");
        return -1;
    }

    args->seed = (unsigned int)atoi(argv[3]);

    args->task = atoi(argv[4]);
    if (args->task != 1 && args->task != 2) {
        fprintf(stderr, "Error: Task must be 1 (max) or 2 (sum)\n");
        return -1;
    }

    if (strcmp(argv[5], "Y") == 0) {
        args->print_results = 1;
    } else if (strcmp(argv[5], "N") == 0) {
        args->print_results = 0;
    } else {
        fprintf(stderr, "Error: Print results must be Y or N\n");
        return -1;
    }

    return 0;
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

	for(i = 0; i < thread_ct; i++) {
		pthread_create(&tids[i], NULL, sum, &args);	
	}	

	for(i = 0; i < thread_ct; i++) {
		pthread_join(tids[i], NULL);	
	}
	
	pthread_mutex_destroy(&mutex);	
	free(tids);

	return total;
}



typedef struct {
    int *start;
    int length;
    int max_value;
} MaxArgs;


void *find_max(void *context) {
    MaxArgs *section = (MaxArgs *)context;
    int *array = section->start;
    int length = section->length;
    int local_max = array[0];
    int k;
    for(k = 1; k < length; k++) {
        if (array[k] > local_max) {
            local_max = array[k];
        }
    }
    section->max_value = local_max;
    return NULL;
}



int run_max(int *arr, int arr_size, int thread_ct) {
    int i, overall_max;
    pthread_t *threads = malloc(thread_ct * sizeof(pthread_t));
    MaxArgs *contexts = malloc(thread_ct * sizeof(MaxArgs));

    int base_len = arr_size / thread_ct;
    int remainder = arr_size % thread_ct;

    int offset = 0;
    for(i = 0; i < thread_ct; i++) {
        contexts[i].start = arr + offset;
        contexts[i].length = base_len + (i < remainder ? 1 : 0);
        contexts[i].max_value = arr[offset];

        if (pthread_create(&threads[i], NULL, find_max, &contexts[i]) != 0) {
            perror("Failed to create thread");
            free(threads);
            free(contexts);
            return -1;
        }
        offset += contexts[i].length;
    }

    overall_max = arr[0];
    for(i = 0; i < thread_ct; i++) {
        pthread_join(threads[i], NULL);
        if (contexts[i].max_value > overall_max) {
            overall_max = contexts[i].max_value;
        }
    }

    free(threads);
    free(contexts);
    return overall_max;
}


int main(int argc, char *argv[]) {
    
    CommandLineArgs args = {0};
    int arr_size, thread_ct, task, print_results, *arr;
    unsigned int seed;
    int i;
    int result;

    struct rusage start_ru, end_ru;
    struct timeval start_wall, end_wall;
    struct timeval diff_ru_utime, diff_wall, diff_ru_stime;


  
    if(parse_arguments(argc, argv, &args) != 0) {
        return 1;
    }

    arr_size = args.num_elements;
    thread_ct = args.num_threads;
    task = args.task;
    print_results = args.print_results;

    seed = args.seed;
    arr = malloc(arr_size * sizeof(int));

    if(arr == NULL) {
        fprintf(stderr, "Failed to allocate memory for array\n");
        return 1;
    }

    /* Filling array with random integers */
    srand(seed);
    for(i = 0; i < arr_size; i++) {
        arr[i] = rand() % MAX_RAND;
    }

    result = 0;

    if(task == 2) {
        getrusage(RUSAGE_SELF, &start_ru);
        gettimeofday(&start_wall, NULL);

        result = run_sum(arr, arr_size, thread_ct);

        gettimeofday(&end_wall, NULL);
        getrusage(RUSAGE_SELF, &end_ru);

        diff_ru_utime = tv_delta(start_ru.ru_utime, end_ru.ru_utime);
        diff_ru_stime = tv_delta(start_ru.ru_stime, end_ru.ru_stime);
        diff_wall = tv_delta(start_wall, end_wall);

        if (print_results) {
            printf("Sum: %d\n", result);
            printf("User time: %ld.%06ld\n", diff_ru_utime.tv_sec, diff_ru_utime.tv_usec);
            printf("System time: %ld.%06ld\n", diff_ru_stime.tv_sec, diff_ru_stime.tv_usec);
            printf("Wall time: %ld.%06ld\n", diff_wall.tv_sec, diff_wall.tv_usec);
        }
    } else if(task == 1) {
        getrusage(RUSAGE_SELF, &start_ru);
        gettimeofday(&start_wall, NULL);

        result = run_max(arr, arr_size, thread_ct);

        gettimeofday(&end_wall, NULL);
        getrusage(RUSAGE_SELF, &end_ru);

        diff_ru_utime = tv_delta(start_ru.ru_utime, end_ru.ru_utime);
        diff_ru_stime = tv_delta(start_ru.ru_stime, end_ru.ru_stime);
        diff_wall = tv_delta(start_wall, end_wall);

        if (print_results) {
            printf("Maximum value: %d\n", result);
            printf("User time: %ld.%06ld\n", diff_ru_utime.tv_sec, diff_ru_utime.tv_usec);
            printf("System time: %ld.%06ld\n", diff_ru_stime.tv_sec, diff_ru_stime.tv_usec);
            printf("Wall time: %ld.%06ld\n", diff_wall.tv_sec, diff_wall.tv_usec);
        }
    } else {
        fprintf(stderr, "Invalid task\n");
        free(arr);
        return 1;
    }

    free(arr);
    return 0;
}
