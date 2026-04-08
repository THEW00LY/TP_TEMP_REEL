/*
 * Description:  A simple client program to connect to the TCP/IP server thanks to Darren Smith
 */

/*
 * Linux:   compile with gcc -Wall client.c -o client
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <time.h>
#include <assert.h>
#include <errno.h>

#include <pthread.h>
#include <semaphore.h>

#define SOCKET_PORT 10020
#define SOCKET_SERVER "127.0.0.1"

#define NSEC_PER_SEC 1000000000

int fd;
pthread_mutex_t fd_mux;  /* protège les accès au socket uniquement */

pthread_barrier_t barrier;
sem_t sem;

typedef struct {
  double dl;
  double dr;
  double bl;
  int turning;
  double rotation;
} Data;

Data data = {0.0, 0.0, 0.0, 0, 0.0};


struct timespec timespec_normalise(struct timespec ts) {
    while (ts.tv_nsec >= NSEC_PER_SEC)  { ++(ts.tv_sec);  ts.tv_nsec -= NSEC_PER_SEC; }
    while (ts.tv_nsec <= -NSEC_PER_SEC) { --(ts.tv_sec);  ts.tv_nsec += NSEC_PER_SEC; }
    if (ts.tv_nsec < 0) { --(ts.tv_sec); ts.tv_nsec = NSEC_PER_SEC + ts.tv_nsec; }
    return ts;
}

struct timespec timespec_add(struct timespec ts1, struct timespec ts2) {
    ts1 = timespec_normalise(ts1);
    ts2 = timespec_normalise(ts2);
    ts1.tv_sec  += ts2.tv_sec;
    ts1.tv_nsec += ts2.tv_nsec;
    return timespec_normalise(ts1);
}

void sleep_until_next_activation(struct timespec *next_activation) {
    int err;
    do {
        err = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, next_activation, NULL);
    } while (err != 0 && errno == EINTR);
    assert(err == 0);
}
//FONCTION POUR AVANCER TOUT DROIT
//LEVE UN SEMAPHORE QUAND IL RENCONTRE UN MUR
void *go_forward() {
    struct timespec next_activation, task_period;
    task_period.tv_sec  = 1;
    task_period.tv_nsec = 0;

    pthread_barrier_wait(&barrier);
    clock_gettime(CLOCK_MONOTONIC, &next_activation);

    while (1) {
        sleep_until_next_activation(&next_activation);

        if (data.turning) {
            next_activation = timespec_add(next_activation, task_period);
            continue;
        }

        double left_sensor, right_sensor;
        char buffer[256];

        pthread_mutex_lock(&fd_mux);
        send(fd, "S\n", strlen("S\n"), 0);
        int n = recv(fd, buffer, 256, 0);
        pthread_mutex_unlock(&fd_mux);

        buffer[n] = '\0';
        printf("%s\n", buffer);
        sscanf(buffer, "S,%lf ,%lf", &left_sensor, &right_sensor);

        data.dl = left_sensor;
        data.dr = right_sensor;

        if (left_sensor >= 500.0 || right_sensor >= 500.0) {
            data.turning = 1;

            pthread_mutex_lock(&fd_mux);
            send(fd, "M,0,0\n", 6, 0);
            pthread_mutex_unlock(&fd_mux);

            sem_post(&sem);
        } else {
            pthread_mutex_lock(&fd_mux);
            send(fd, "M,60,60\n", strlen("M,60,60\n"), 0);
            pthread_mutex_unlock(&fd_mux);
        }

        next_activation = timespec_add(next_activation, task_period);
    }
}

//FONCTION POUR FAIRE DEMI TOUR
//SE LANCE A CHAQUE FOIS QUE LE SEMAPHORE EST RAISED
void *turn() {
    pthread_barrier_wait(&barrier);

    while (1) {
        sem_wait(&sem);

        char buffer[256];
        char buf[64];

        data.rotation = fmod(data.rotation + 3.14, 6.28);
        snprintf(buf, sizeof(buf), "T,%.2f\n", data.rotation);

        pthread_mutex_lock(&fd_mux);
        send(fd, buf, strlen(buf), 0);
        send(fd, "M,60,60\n", strlen("M,60,60\n"), 0);
        int n = recv(fd, buffer, sizeof(buffer), 0);
        pthread_mutex_unlock(&fd_mux);

        (void)n;
        data.turning = 0;
    }
}
//DISPLAY SA BATTERY LEVEL
void *display_battery() {
    struct timespec next_activation, task_period;
    task_period.tv_sec  = 5;   /* toutes les 5 secondes */
    task_period.tv_nsec = 0;

    pthread_barrier_wait(&barrier);
    clock_gettime(CLOCK_MONOTONIC, &next_activation);

    while (1) {
        sleep_until_next_activation(&next_activation);

        double battery_level;
        char buffer[256];

        pthread_mutex_lock(&fd_mux);
        send(fd, "B\n", strlen("B\n"), 0);
        int n = recv(fd, buffer, 256, 0);
        pthread_mutex_unlock(&fd_mux);

        buffer[n] = '\0';
        printf("BATTERY: %s\n", buffer);
        sscanf(buffer, "B ,%lf", &battery_level);

        data.bl = battery_level;

        if (battery_level < 50) {
            sem_post(&sem);
        }

        next_activation = timespec_add(next_activation, task_period);
    }
}

int main(int argc, char *argv[]) {

    struct sockaddr_in address;
    const struct hostent *server;
    int rc;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        printf("cannot create socket\n");
        return -1;
    }

    memset(&address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_port = htons(SOCKET_PORT);
    server = gethostbyname(SOCKET_SERVER);

    if (server)
        memcpy((char *)&address.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
    else {
        printf("cannot resolve server name: %s\n", SOCKET_SERVER);
        close(fd);
        return -1;
    }

    rc = connect(fd, (struct sockaddr *)&address, sizeof(struct sockaddr));
    if (rc == -1) {
        printf("cannot connect to the server\n");
        close(fd);
        return -1;
    }

    fflush(stdout);

    sem_init(&sem, 0, 0);
    pthread_mutex_init(&fd_mux, NULL);
    pthread_barrier_init(&barrier, NULL, 3);

    pthread_t t1, t2, t3;
    pthread_attr_t attr1, attr2, attr3;
    struct sched_param param1, param2, param3;

    pthread_attr_init(&attr1);
    pthread_attr_init(&attr2);
    pthread_attr_init(&attr3);

    pthread_attr_setschedpolicy(&attr1, SCHED_RR);
    pthread_attr_setschedpolicy(&attr2, SCHED_RR);
    pthread_attr_setschedpolicy(&attr3, SCHED_RR);

    pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setinheritsched(&attr3, PTHREAD_EXPLICIT_SCHED);

    param1.sched_priority = 99;
    param2.sched_priority = 98;
    param3.sched_priority = 1;

    pthread_attr_setschedparam(&attr1, &param1);
    pthread_attr_setschedparam(&attr2, &param2);
    pthread_attr_setschedparam(&attr3, &param3);

    pthread_create(&t1, &attr1, go_forward, NULL);
    pthread_create(&t2, &attr2, turn, NULL);
    pthread_create(&t3, &attr3, display_battery, NULL);

    pthread_attr_destroy(&attr1);
    pthread_attr_destroy(&attr2);
    pthread_attr_destroy(&attr3);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    pthread_barrier_destroy(&barrier);
    pthread_mutex_destroy(&fd_mux);
    close(fd);

    return 0;
}
