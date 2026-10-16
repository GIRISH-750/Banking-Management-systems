#define _GNU_SOURCE        /* needed for F_OFD_SETLKW */
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "lock.h"

static int set_lock(int fd, int id, size_t size, short type) {
    struct flock fl;
    memset(&fl, 0, sizeof(fl));      /* l_pid MUST be 0 for OFD locks */
    fl.l_type   = type;
    fl.l_whence = SEEK_SET;
    fl.l_start  = (off_t)(id - 1) * (off_t)size;
    fl.l_len    = (off_t)size;       /* lock only this one record */

    while (fcntl(fd, F_OFD_SETLKW, &fl) < 0) {
        if (errno != EINTR) {        /* retry if a signal interrupted the wait */
            return -1;
        }
    }
    return 0;
}

int lock_record(int fd, int id, size_t size, short type) {
    return set_lock(fd, id, size, type);
}

int unlock_record(int fd, int id, size_t size) {
    return set_lock(fd, id, size, F_UNLCK);
}
