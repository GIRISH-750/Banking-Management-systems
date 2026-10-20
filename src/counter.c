#include <fcntl.h>
#include <unistd.h>
#include "counter.h"
#include "entities.h"
#include "lock.h"

int counter_next(size_t field) {
    struct Counter c;
    int fd = open(COUNTER_FILE, O_RDWR);
    if (fd < 0) {
        return -1;
    }

    lock_record(fd, 1, sizeof(c), F_WRLCK);      /* counter.bin has one record */
    int id = -1;
    if (pread(fd, &c, sizeof(c), 0) == (ssize_t)sizeof(c)) {
        int* slot = (int*)((char*)&c + field);    /* the field we were asked for */
        id = *slot;
        (*slot)++;
        if (pwrite(fd, &c, sizeof(c), 0) != (ssize_t)sizeof(c) || fsync(fd) != 0) {
            id = -1;
        }
    }
    unlock_record(fd, 1, sizeof(c));
    close(fd);
    return id;
}
