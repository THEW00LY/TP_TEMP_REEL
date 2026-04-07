#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <stdint.h>
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
	/* normalize tv_nsec to be < 1e9 */
	if (next_activation->tv_nsec >= 1000000000L) {
		next_activation->tv_sec += next_activation->tv_nsec / 1000000000L;
		next_activation->tv_nsec = next_activation->tv_nsec % 1000000000L;
	}
}
void *task1(void *arg) {
	pthread_barrier_wait(&barrier);
	struct timespec next_activation, task_period;
	set_task_period(&task_period, 1);
	int err = clock_gettime(CLOCK_MONOTONIC, &next_activation);
	
	assert(err == 0);

	while (1) {
		sleep_until_next_activation(&next_activation);

		struct timespec t_start, t_end;
		if (clock_gettime(CLOCK_MONOTONIC, &t_start) != 0) {
			perror("clock_gettime");
		}

		/* simulated workload - adjust to make scheduler effects visible */
		volatile unsigned long dummy = 0;
		for (unsigned long i = 0; i < load * 10000ul; ++i) {
			dummy += i;
		}
		(void)dummy;

		printf("process1\n");

		if (clock_gettime(CLOCK_MONOTONIC, &t_end) != 0) {
			perror("clock_gettime");
		}

		long long resp_us = (t_end.tv_sec - t_start.tv_sec) * 1000000LL +
			(t_end.tv_nsec - t_start.tv_nsec) / 1000LL;
		printf("Response time task 1: %lld (us)\n", resp_us);
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

		struct timespec t_start, t_end;
		if (clock_gettime(CLOCK_MONOTONIC, &t_start) != 0) {
			perror("clock_gettime");
		}

		/* simulated workload - can be adjusted */
		volatile unsigned long dummy = 0;
		for (unsigned long i = 0; i < load * 10000ul; ++i) {
			dummy += i;
		}
		(void)dummy;

		printf("process2\n");

		if (clock_gettime(CLOCK_MONOTONIC, &t_end) != 0) {
			perror("clock_gettime");
		}

		long long resp_us = (t_end.tv_sec - t_start.tv_sec) * 1000000LL +
			(t_end.tv_nsec - t_start.tv_nsec) / 1000LL;
		printf("Response time task 2: %lld (us)\n", resp_us);
		timespec_add(&next_activation, &task_period);
		//pthread_exit(NULL);
	}
};

void *task3(void *arg) {
	pthread_barrier_wait(&barrier);
	struct timespec next_activation, task_period;
	set_task_period(&task_period, 2);
	int err = clock_gettime(CLOCK_MONOTONIC, &next_activation);
	
	assert(err == 0);

	while (1) {
		sleep_until_next_activation(&next_activation);

		struct timespec t_start, t_end;
		if (clock_gettime(CLOCK_MONOTONIC, &t_start) != 0) {
			perror("clock_gettime");
		}

		/* simulated workload - can be adjusted */
		volatile unsigned long dummy = 0;
		for (unsigned long i = 0; i < load * 10000ul; ++i) {
			dummy += i;
		}
		(void)dummy;

		printf("process3\n");

		if (clock_gettime(CLOCK_MONOTONIC, &t_end) != 0) {
			perror("clock_gettime");
		}

		long long resp_us = (t_end.tv_sec - t_start.tv_sec) * 1000000LL +
			(t_end.tv_nsec - t_start.tv_nsec) / 1000LL;
		printf("Response time task 2: %lld (us)\n", resp_us);
		timespec_add(&next_activation, &task_period);
		//pthread_exit(NULL);
	}
};
//----------------------------------------------------------------------------------------------------------
//question 2)
/* int main (void) {
	pthread_attr_t attr;
    if (pthread_barrier_init(&barrier, NULL, 2) != 0) {
        perror("Erreur d'initialisation de la barrière");
        return EXIT_FAILURE;
    }
	pthread_t tid1;
	if (pthread_create (&tid1, &attr, task1, NULL) != 0){
		printf("ne marche pas\n");
		return -1;
	};
	pthread_t tid2;
	if (pthread_create (&tid2, &attr, task2, NULL) != 0){
		printf("ne marche pas\n");
		return -1;
	};
	while (1) {

	}
} */
//----------------------------------------------------------------------------------------------------------
//question 3) exec with: sudo taskset -c 0 ./pre_project
//----------------------------------------------------------------------------------------------------------
//question 4) Here we have a stable exec time for task 2 if its priority is < of task 1's
//prio bc it can't be preempted in this case, but if they have the same prio, task 2's
//exec time starting being non deterministic

/* int main (void) {
	//attr init
	pthread_attr_t attr;
	int ret = pthread_attr_init(&attr);
	if (ret != 0) {
		fprintf(stderr, "pthread_attr_init: %s\n", strerror(ret));
		return EXIT_FAILURE;
	}

	ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
	if (ret != 0) {
		fprintf(stderr, "pthread_attr_setinheritsched: %s\n", strerror(ret));
	}

	ret = pthread_attr_setschedpolicy(&attr, SCHED_RR);
	if (ret != 0) {
		fprintf(stderr, "pthread_attr_setschedpolicy: %s\n", strerror(ret));
	}

	//get valid priority range for the chosen policy
	int prio_max = sched_get_priority_max(SCHED_RR);
	int prio_min = sched_get_priority_min(SCHED_RR);
	if (prio_max == -1 || prio_min == -1) {
		perror("sched_get_priority_max/min");
	}

	//prepare parameters - default: make task1 higher than task2 if possible
	struct sched_param parameter;
	int p_high = prio_max;
	int p_low = (prio_max > prio_min) ? (prio_max - 1) : prio_min;

	if (pthread_barrier_init(&barrier, NULL, 2) != 0) {
		perror("Erreur d'initialisation de la barrière");
		return EXIT_FAILURE;
	}

	pthread_t tid1, tid2;

	//create task1 with higher priority
	parameter.sched_priority = p_high;
	ret = pthread_attr_setschedparam(&attr, &parameter);
	if (ret != 0) {
		fprintf(stderr, "pthread_attr_setschedparam(task1): %s\n", strerror(ret));
	}

	ret = pthread_create(&tid1, &attr, task1, NULL);
	if (ret != 0) {
		fprintf(stderr, "pthread_create tid1: %s\n", strerror(ret));
		return -1;
	}

	//print actual scheduling of tid1
	int pol; struct sched_param actual;
	if (pthread_getschedparam(tid1, &pol, &actual) == 0) {
		printf("tid1 policy=%d priority=%d\n", pol, actual.sched_priority);
	}

	//create task2 with lower priority
	parameter.sched_priority = p_high;
	ret = pthread_attr_setschedparam(&attr, &parameter);
	if (ret != 0) {
		fprintf(stderr, "pthread_attr_setschedparam(task2): %s\n", strerror(ret));
	}

	ret = pthread_create(&tid2, &attr, task2, NULL);
	if (ret != 0) {
		fprintf(stderr, "pthread_create tid2: %s\n", strerror(ret));
		return -1;
	}

	if (pthread_getschedparam(tid2, &pol, &actual) == 0) {
		printf("tid2 policy=%d priority=%d\n", pol, actual.sched_priority);
	}

	//cleanup attributes and wait for threads
	pthread_attr_destroy(&attr);

	pthread_join(tid1, NULL);
	pthread_join(tid2, NULL);

	pthread_barrier_destroy(&barrier);
	return 0;
} */
//----------------------------------------------------------------------------------------------------------

int main (void) {
	/* attr init */
	pthread_attr_t attr;
	int ret = pthread_attr_init(&attr);
	if (ret != 0) {
		fprintf(stderr, "pthread_attr_init: %s\n", strerror(ret));
		return EXIT_FAILURE;
	}

	ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
	if (ret != 0) {
		fprintf(stderr, "pthread_attr_setinheritsched: %s\n", strerror(ret));
	}

	ret = pthread_attr_setschedpolicy(&attr, SCHED_RR);
	if (ret != 0) {
		fprintf(stderr, "pthread_attr_setschedpolicy: %s\n", strerror(ret));
	}

	/* get valid priority range for the chosen policy */
	int prio_max = sched_get_priority_max(SCHED_RR);
	int prio_min = sched_get_priority_min(SCHED_RR);
	if (prio_max == -1 || prio_min == -1) {
		perror("sched_get_priority_max/min");
	}

	/* prepare parameters - default: make task1 higher than task2 if possible */
	struct sched_param parameter;
	int p_high = prio_max;
	int p_mid = (prio_max > prio_min) ? (prio_max - 1) : prio_min;
	int p_low = (p_mid > prio_min) ? (p_mid - 1) : prio_min;

	if (pthread_barrier_init(&barrier, NULL, 2) != 0) {
		perror("Erreur d'initialisation de la barrière");
		return EXIT_FAILURE;
	}

	pthread_t tid1, tid2, tid3;

	/* create task1 with higher priority */
	parameter.sched_priority = p_high;
	ret = pthread_attr_setschedparam(&attr, &parameter);
	if (ret != 0) {
		fprintf(stderr, "pthread_attr_setschedparam(task1): %s\n", strerror(ret));
	}

	ret = pthread_create(&tid1, &attr, task1, NULL);
	if (ret != 0) {
		fprintf(stderr, "pthread_create tid1: %s\n", strerror(ret));
		return -1;
	}

	/* print actual scheduling of tid1 */
	int pol; struct sched_param actual;
	if (pthread_getschedparam(tid1, &pol, &actual) == 0) {
		printf("tid1 policy=%d priority=%d\n", pol, actual.sched_priority);
	}

	/* create task2 with lower priority */
	parameter.sched_priority = p_low;
	ret = pthread_attr_setschedparam(&attr, &parameter);
	if (ret != 0) {
		fprintf(stderr, "pthread_attr_setschedparam(task2): %s\n", strerror(ret));
	}

	ret = pthread_create(&tid2, &attr, task2, NULL);
	if (ret != 0) {
		fprintf(stderr, "pthread_create tid2: %s\n", strerror(ret));
		return -1;
	}

	if (pthread_getschedparam(tid2, &pol, &actual) == 0) {
		printf("tid2 policy=%d priority=%d\n", pol, actual.sched_priority);
	}

	/* create task3 with mid priority */
	parameter.sched_priority = p_mid;
	ret = pthread_attr_setschedparam(&attr, &parameter);
	if (ret != 0) {
		fprintf(stderr, "pthread_attr_setschedparam(task2): %s\n", strerror(ret));
	}

	ret = pthread_create(&tid3, &attr, task2, NULL);
	if (ret != 0) {
		fprintf(stderr, "pthread_create tid2: %s\n", strerror(ret));
		return -1;
	}

	if (pthread_getschedparam(tid3, &pol, &actual) == 0) {
		printf("tid2 policy=%d priority=%d\n", pol, actual.sched_priority);
	}

	/* cleanup attributes and wait for threads */
	pthread_attr_destroy(&attr);

	pthread_join(tid1, NULL);
	pthread_join(tid2, NULL);
	pthread_join(tid3, NULL);

	pthread_barrier_destroy(&barrier);
	return 0;
}
