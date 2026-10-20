#ifndef CONFIG_H
#define CONFIG_H

#define SERVER_PORT  5000
#define MAX_CLIENTS  100    /* counting semaphore initial value (TA requirement) */
#define POOL_SIZE    MAX_CLIENTS  /* worker threads: one per live session */
#define MAX_LINE_LEN 256

#endif
