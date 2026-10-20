#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include "rec_store.h"

int rec_open(const char* path) {
    return open(path, O_RDWR);
}

int rec_read(int fd, int id, void* buf, size_t size) {
    if (id <= 0) {
        return REC_NOT_FOUND;
    }
    ssize_t n = pread(fd, buf, size, (off_t)(id - 1) * (off_t)size);
    if (n < 0) {
        return REC_ERROR;
    }
    return ((size_t)n < size) ? REC_NOT_FOUND : REC_OK;
}

int rec_write(int fd, int id, const void* buf, size_t size) {
    ssize_t n = pwrite(fd, buf, size, (off_t)(id - 1) * (off_t)size);
    if (n != (ssize_t)size) {
        return REC_ERROR;
    }
    return (fsync(fd) == 0) ? REC_OK : REC_ERROR;   /* durable before OK */
}
