#include <stdio.h>
#include <pthread.h>
#include "config.h"
#include "thread_pool.h"

/* Circular queue of client fds, protected by q_mutex. */
static int q_buf[POOL_QUEUE_CAP];
static int q_head = 0, q_tail = 0, q_count = 0;
static int n_workers = 0;   /* threads started */
static int n_busy = 0;      /* workers serving a client right now */
static pool_job_fn job_fn = NULL;

static pthread_mutex_t q_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  not_empty = PTHREAD_COND_INITIALIZER;   /* workers wait here */
static pthread_cond_t  not_full  = PTHREAD_COND_INITIALIZER;   /* accept loop waits here */

static void* worker(void* arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&q_mutex);
        while (q_count == 0) {   /* while, not if: spurious wakeups */
            pthread_cond_wait(&not_empty, &q_mutex);
        }
        int fd = q_buf[q_head];
        q_head = (q_head + 1) % POOL_QUEUE_CAP;
        q_count--;
        n_busy++;
        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&q_mutex);

        job_fn(fd);   /* whole client session, OUTSIDE the queue lock */

        pthread_mutex_lock(&q_mutex);
        n_busy--;
        pthread_mutex_unlock(&q_mutex);
    }
    return NULL;
}

int pool_init(int nworkers, pool_job_fn fn) {
    if (nworkers < 1 || fn == NULL) {
        return -1;
    }
    job_fn = fn;
    int started = 0;
    for (int i = 0; i < nworkers; i++) {
        pthread_t tid;
        if (pthread_create(&tid, NULL, worker, NULL) != 0) {
            perror("pthread_create");
            break;
        }
        pthread_detach(tid);
        started++;
    }
    pthread_mutex_lock(&q_mutex);
    n_workers = started;
    pthread_mutex_unlock(&q_mutex);
    return started > 0 ? started : -1;
}

int pool_submit(int fd) {
    pthread_mutex_lock(&q_mutex);
    while (q_count == POOL_QUEUE_CAP) {
        pthread_cond_wait(&not_full, &q_mutex);
    }
    q_buf[q_tail] = fd;
    q_tail = (q_tail + 1) % POOL_QUEUE_CAP;
    q_count++;
    int queued = (q_count > n_workers - n_busy);   /* more jobs than idle workers */
    pthread_cond_signal(&not_empty);
    pthread_mutex_unlock(&q_mutex);
    return queued ? POOL_QUEUED : POOL_STARTED;
}

int pool_waiting(void) {
    pthread_mutex_lock(&q_mutex);
    int w = q_count - (n_workers - n_busy);
    pthread_mutex_unlock(&q_mutex);
    return w > 0 ? w : 0;
}
