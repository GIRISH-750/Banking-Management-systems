#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stddef.h>
#include "txn_store.h"
#include "counter.h"
#include "lock.h"

int txn_log(int customer_id, const char* type, float amount) {
    struct Transaction t;
    memset(&t, 0, sizeof(t));

    int id = counter_next(offsetof(struct Counter, next_transaction_id));
    if (id < 1) {
        return -1;
    }
    t.transactionID = id;
    t.customerID = customer_id;
    strncpy(t.type, type, TYPE_LEN - 1);
    t.amount = amount;

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);                       /* _r = thread-safe version */
    strftime(t.timestamp, TIMESTAMP_LEN, "%Y-%m-%d %H:%M:%S", &tm_now);

    int fd = open(TXN_FILE, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        return -1;
    }
    /* Each transaction has its own slot at (id-1)*size, like every other
       file, so two writers with different IDs never touch the same bytes. */
    lock_record(fd, id, sizeof(t), F_WRLCK);
    ssize_t n = pwrite(fd, &t, sizeof(t), (off_t)(id - 1) * (off_t)sizeof(t));
    unlock_record(fd, id, sizeof(t));
    close(fd);
    return (n == (ssize_t)sizeof(t)) ? 0 : -1;
}

int txn_history(int customer_id, struct Transaction* out, int max) {
    int fd = open(TXN_FILE, O_RDONLY | O_CREAT, 0644);
    if (fd < 0) {
        return -1;
    }

    lock_file(fd, F_RDLCK);          /* nobody writes while we scan */
    int count = 0;
    struct Transaction t;
    off_t off = 0;
    while (pread(fd, &t, sizeof(t), off) == (ssize_t)sizeof(t)) {
        off += sizeof(t);
        if (t.customerID != customer_id || t.transactionID == 0) {
            continue;                 /* someone else's, or an empty slot */
        }
        if (count < max) {
            out[count++] = t;
        } else {                      /* keep only the latest `max`: shift left */
            memmove(out, out + 1, (size_t)(max - 1) * sizeof(t));
            out[max - 1] = t;
        }
    }
    unlock_file(fd);
    close(fd);
    return count;
}
