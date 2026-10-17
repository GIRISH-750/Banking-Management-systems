/* DEMO: what happens WITHOUT lock ordering. Thread A locks 1 then wants 2,
   thread B locks 2 then wants 1. Both wait forever. Stop with Ctrl+C. */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include "lock.h"
#include "entities.h"

static void* worker(void* arg) {
    int first = *(int*)arg;
    int second = (first == 1) ? 2 : 1;
    int fd = open("data/customers.bin", O_RDWR);
    lock_record(fd, first, sizeof(struct Customer), F_WRLCK);
    printf("thread holding %d, now waiting for %d...\n", first, second);
    fflush(stdout);
    sleep(1);
    lock_record(fd, second, sizeof(struct Customer), F_WRLCK);
    printf("thread got both (you will never see this)\n");
    return NULL;
}

int main(void) {
    int a = 1, b = 2;
    pthread_t t1, t2;
    pthread_create(&t1, NULL, worker, &a);
    pthread_create(&t2, NULL, worker, &b);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    return 0;
}
