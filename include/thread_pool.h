#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "config.h"

#define POOL_QUEUE_CAP MAX_CLIENTS   /* semaphore never lets more than this in */

#define POOL_STARTED 0   /* an idle worker will take it now */
#define POOL_QUEUED  1   /* all workers busy, client waits in the queue */

typedef void (*pool_job_fn)(int fd);

/* Start nworkers threads that run fn(fd) for each queued client.
   Returns the number of workers started, or -1 on failure. */
int pool_init(int nworkers, pool_job_fn fn);

/* Put a client fd in the queue (blocks only if the queue is full). */
int pool_submit(int fd);

/* How many clients are waiting for a free worker right now. */
int pool_waiting(void);

#endif
