/*
 * Description:  A simple client program to connect to the TCP/IP server thanks to Darren Smith
 */

/*
 * Linux:   compile with gcc -Wall client.c -o client
 */

#include <stdio.h>
#include <string.h>

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
#define SOCKET_SERVER "127.0.0.1" /* local host */

#define NSEC_PER_SEC 1000000000

int fd; 
pthread_mutex_t mux;
pthread_barrier_t barrier;
sem_t wall_detected;

typedef struct {
  double dl;
  double dr;
  int turning;
} Data;

Data data = {0.0, 0.0, 0};

/* static int send_cmd(const char *cmd, char *reply, size_t reply_len) {
    if (send(fd, cmd, strlen(cmd), 0) < 0) { perror("send"); return -1; }
    if (reply != NULL) {
        int n = recv(fd, reply, (int)reply_len - 1, 0);
        if (n <= 0) { perror("recv"); return -1; }
        reply[n] = '\0';
    }
    return 0;
} */

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

//avance et check si on est en face d'un mur constamment
void *go_forward () {
  //on declare en local afin d'eviter d'utiliser la meme ressource avec les 2 foncs
  struct timespec next_activation, task_period;
  task_period.tv_sec  = 1;  
  task_period.tv_nsec = 0;
  pthread_barrier_wait(&barrier);
  int is_it_turning = 0;
  clock_gettime(CLOCK_MONOTONIC, &next_activation);
  while(1) {
    sleep_until_next_activation(&next_activation);

    //vérif si on tourne:
    pthread_mutex_lock(&mux);
    is_it_turning = data.turning;
    pthread_mutex_unlock(&mux);
    if (is_it_turning) {
      continue;
    }

    //vérif la distance devant nous:
    double left_sensor ,right_sensor;
    char buffer [256];
    send (fd, "S\n", strlen("S\n") ,0);
    int n = recv(fd, buffer, 256, 0) ;
    buffer[n] = '\0';
    printf("%s\n", buffer);
    sscanf(buffer,"S,%lf ,%lf", &left_sensor, &right_sensor);

    //on la met dans nos data partagées
    pthread_mutex_lock(&mux);
    data.dl = left_sensor;
    data.dr = right_sensor;
    pthread_mutex_unlock(&mux);
    if (left_sensor >= 500.0 || right_sensor >= 500.0){
      pthread_mutex_lock(&mux);
      data.turning = 1;
      pthread_mutex_unlock(&mux);
      // Arrêter les moteurs avant de tourner
      send(fd, "M,0,0\n", 6, 0);
      sem_post(&wall_detected);  // ← réveille turn()
    }
    else {
      //on avance
      send(fd,"M,60,60\n",strlen("M,60,60\n"),0);
    }
    next_activation = timespec_add(next_activation, task_period);
  }

}

void *turn () {
  pthread_barrier_wait(&barrier);
  while(1) {
    sem_wait(&wall_detected);
    //turn
    int buffer[64];
    send(fd, "T,3.14\n" , strlen("T ,3.14\n") ,0);
    send(fd,"M,60,60\n",strlen("M,60,60\n"),0);
    int n = recv(fd, buffer, 256, 0);

    pthread_mutex_lock(&mux);
    data.turning = 0;
    pthread_mutex_unlock(&mux);
  }
}

int main(int argc, char *argv[]) {
  
  //--------------//
  struct sockaddr_in address;
  const struct hostent *server;
  int rc;
  char buffer[256];

  /* create the socket */
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    printf("cannot create socket\n");
    return -1;
  }

  /* fill in the socket address */
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

  /* connect to the server */
  rc = connect(fd, (struct sockaddr *)&address, sizeof(struct sockaddr));
  if (rc == -1) {
    printf("cannot connect to the server\n");
    close(fd);
    return -1;
  }

  fflush(stdout);

  //send(fd,"M,0,99\n",strlen("M,0,99\n"),0);

/*   for (;;) {
    int n = recv(fd, buffer, 256, 0);
    buffer[n] = '\0';
    printf("Received: %s", buffer);
  } */
  
  //partie THREAD//
  sem_init (&wall_detected, 0, 0) ;
  // Initialize the barrier for 2 threads
  pthread_barrier_init(&barrier, NULL, 2);

  pthread_t t1, t2;
  pthread_attr_t attr1, attr2;
  struct sched_param param1, param2;

  // Initialize thread attributes
  pthread_attr_init(&attr1);
  pthread_attr_init(&attr2);

  // Set scheduling policy to SCHED_RR
  pthread_attr_setschedpolicy(&attr1, SCHED_RR);
  pthread_attr_setschedpolicy(&attr2, SCHED_RR);

  // on fait en sorte de ne pas heriter le scheduling du main
  pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);

  // on set nos prioritées.
  param1.sched_priority = 99; // High priority for t1
  param2.sched_priority = 98;  // Low priority for t2

  pthread_attr_setschedparam(&attr1, &param1);
  pthread_attr_setschedparam(&attr2, &param2);

  // creation des tthreads
  pthread_create(&t1, &attr1, go_forward, NULL);
  pthread_create(&t2, &attr2, turn, NULL);

  
  pthread_attr_destroy(&attr1);
  pthread_attr_destroy(&attr2);
  //attente fin threads
  pthread_join(t1, NULL);
  pthread_join(t2, NULL);

  // Destroy barrier
  pthread_barrier_destroy(&barrier);
  close(fd);

  return 0;
}
