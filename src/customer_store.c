#include <fcntl.h>
#include <unistd.h>
#include "customer_store.h"

static off_t offset_of(int id) {
    return (off_t)(id - 1) * (off_t)sizeof(struct Customer);
}

int customer_open(void) {
    return open(CUSTOMERS_FILE, O_RDWR);
}

int customer_read(int fd, int id, struct Customer* c) {
    if (id < 1) {
        return STORE_NOT_FOUND;
    }
    ssize_t n = pread(fd, c, sizeof(*c), offset_of(id));
    if (n < 0) {
        return STORE_ERROR;
    }
    if ((size_t)n < sizeof(*c) || c->userID != id) {
        return STORE_NOT_FOUND;       /* past end of file, or empty slot */
    }
    return STORE_OK;
}

int customer_write(int fd, int id, const struct Customer* c) {
    ssize_t n = pwrite(fd, c, sizeof(*c), offset_of(id));
    if (n != (ssize_t)sizeof(*c)) {
        return STORE_ERROR;
    }
    return STORE_OK;
}
