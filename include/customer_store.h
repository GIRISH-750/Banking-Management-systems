#ifndef CUSTOMER_STORE_H
#define CUSTOMER_STORE_H

#include "entities.h"

#define CUSTOMERS_FILE "data/customers.bin"

#define STORE_OK         0
#define STORE_NOT_FOUND  1
#define STORE_ERROR     -1

/* Opens customers.bin for read+write. Each operation must open its own fd
   (OFD locks belong to the fd). Returns fd, or -1 on error. */
int customer_open(void);

/* Reads customer `id` into *c. Caller must already hold a lock on the record. */
int customer_read(int fd, int id, struct Customer* c);

/* Writes *c to record `id`. Caller must already hold a WRITE lock. */
int customer_write(int fd, int id, const struct Customer* c);

#endif
