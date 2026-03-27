#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <bits/pthreadtypes.h>

#define load 10000

/*struct timespec {
 time_t tv_sec ;  Seconds 
 long tv_nsec ;  Nanoseconds 
}; */

pthread_barrier_t barrier;

void sleep_until_next_activation(struct timespec * next_activation) {
	int err;
	do {
	// absolute sleep until next_activation
		err = clock_nanosleep(	CLOCK_MONOTONIC,
					TIMER_ABSTIME,
					next_activation, 
					NULL);
	// if err is nonzero , we might have woken up too early
	} while (err !=0 && errno == EINTR);
	assert (err == 0);
}
void set_task_period(struct timespec * task_period, long periodSec) {
	task_period->tv_nsec = 0;
	task_period->tv_sec = periodSec;
}

void timespec_add(struct timespec * next_activation, struct timespec * task_period) {
	next_activation->tv_sec += task_period->tv_sec;
	next_activation->tv_nsec += task_period->tv_nsec;
}
void *task1(void *arg) {
	pthread_barrier_wait(&barrier);
	struct timespec next_activation, task_period;
	set_task_period(&task_period, 1);
	int err = clock_gettime(CLOCK_MONOTONIC, &next_activation);
	
	assert(err == 0);

	while (1) {
		sleep_until_next_activation(&next_activation);
        
		clock_t begin = clock();
		printf("process1 \n");
		clock_t end = clock();
		unsigned long resp_time = (end - begin);
		printf("Response time: %ld (us) \n", resp_time);
		timespec_add(&next_activation, &task_period);

		
		//pthread_exit(NULL);
	}
};

void *task2(void *arg) {
	pthread_barrier_wait(&barrier);
	struct timespec next_activation, task_period;
	set_task_period(&task_period, 2);
	int err = clock_gettime(CLOCK_MONOTONIC, &next_activation);
	
	assert(err == 0);

	while (1) {
		sleep_until_next_activation(&next_activation);
        
		clock_t begin = clock();
		printf("process2 \n");
		clock_t end = clock();
		unsigned long resp_time = (end - begin);
		printf("Response time: %ld (us) \n", resp_time);
		timespec_add(&next_activation, &task_period);
		//pthread_exit(NULL);
	}
};

int main (void) {
	
    if (pthread_barrier_init(&barrier, NULL, 2) != 0) {
        perror("Erreur d'initialisation de la barrière");
        return EXIT_FAILURE;
    }
	pthread_t tid1;
	if (pthread_create (&tid1, NULL, task1, NULL) != 0){
			printf("ne marche pas\n");
			return -1;
	};
	pthread_t tid2;
	if (pthread_create (&tid2, NULL, task2, NULL) != 0){
			printf("ne marche pas\n");
			return -1;
	};
	while (1) {

	}
}

