/* Two threads write-lock the same record. Thread B must wait for A. */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "lock.h"
#include "entities.h"

#define FILE_PATH "data/customers.bin"

static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

static double start;

static void* worker(void* arg) {
    const char* name = arg;
    int fd = open(FILE_PATH, O_RDWR);          /* each thread opens its own fd */
    if (fd < 0) { perror("open"); return NULL; }

    lock_record(fd, 1, sizeof(struct Customer), F_WRLCK);
    printf("[%.2fs] %s got lock on customer 1\n", now() - start, name);
    sleep(2);                                  /* hold it */
    unlock_record(fd, 1, sizeof(struct Customer));
    printf("[%.2fs] %s released lock\n", now() - start, name);

    close(fd);
    return NULL;
}

int main(void) {
    pthread_t a, b;
    start = now();
    pthread_create(&a, NULL, worker, "Thread A");
    usleep(100000);                            /* make sure A goes first */
    pthread_create(&b, NULL, worker, "Thread B");
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    printf("PASS if B got the lock only after A released (~2s)\n");
    return 0;
}
