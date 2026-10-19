#ifndef REC_STORE_H
#define REC_STORE_H
#include <stddef.h>

#define EMPLOYEES_FILE "data/employees.bin"
#define MANAGERS_FILE  "data/managers.bin"
#define ADMINS_FILE    "data/admins.bin"

#define REC_OK         0
#define REC_NOT_FOUND  1
#define REC_ERROR     -1

/* Any fixed-size record file: record `id` sits at (id-1)*size.
   Caller must hold the right lock before read/write. */
int rec_open(const char* path);
int rec_read(int fd, int id, void* buf, size_t size);
int rec_write(int fd, int id, const void* buf, size_t size);
#endif
