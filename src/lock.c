#define _GNU_SOURCE        /* needed for F_OFD_SETLKW */
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "lock.h"

static int set_range(int fd, off_t start, off_t len, short type) {
    struct flock fl;
    memset(&fl, 0, sizeof(fl));      /* l_pid MUST be 0 for OFD locks */
    fl.l_type   = type;
    fl.l_whence = SEEK_SET;
    fl.l_start  = start;
    fl.l_len    = len;

    while (fcntl(fd, F_OFD_SETLKW, &fl) < 0) {
        if (errno != EINTR) {        /* retry if a signal interrupted the wait */
            return -1;
        }
    }
    return 0;
}

/* record `id` sits at (id-1)*size and is `size` bytes long */
int lock_record(int fd, int id, size_t size, short type) {
    return set_range(fd, (off_t)(id - 1) * (off_t)size, (off_t)size, type);
}

int unlock_record(int fd, int id, size_t size) {
    return set_range(fd, (off_t)(id - 1) * (off_t)size, (off_t)size, F_UNLCK);
}

int lock_file(int fd, short type) {
    return set_range(fd, 0, 0, type);
}

int unlock_file(int fd) {
    return set_range(fd, 0, 0, F_UNLCK);
}
