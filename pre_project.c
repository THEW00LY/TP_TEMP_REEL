#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>

#define load 10000

/*struct timespec {
 time_t tv_sec ;  Seconds 
 long tv_nsec ;  Nanoseconds 
}; */



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

void process1_activation() {
	printf("process1 \n");
};

/*void process2_activation() {
	printf("process2\n");
}*/

void set_task_period(struct timespec * task_period) {
	task_period->tv_nsec = 0;
	task_period->tv_sec = 1;
}

void timespec_add(struct timespec * next_activation, struct timespec * task_period) {
	next_activation->tv_sec += task_period->tv_sec;
	next_activation->tv_nsec += task_period->tv_nsec;
}

int main (void) {
	struct timespec next_activation, task_period;
	set_task_period(&task_period);
	int err= clock_gettime(CLOCK_MONOTONIC, &next_activation);
	assert(err == 0);
	next_activation.tv_sec = 0;
	while (1) {
	process1_activation();
		sleep_until_next_activation(&next_activation);
		
		process1_activation();
		clock_t end = clock(); //tick par secondes, (CLOCKS_PER_SEC = 1000 000 de tick/s
		clock_t time = ((clock_t) next_activation.tv_sec);
		printf("%ld \n", time);
		timespec_add(&next_activation, &task_period);
	}
}


