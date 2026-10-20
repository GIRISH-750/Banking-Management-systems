#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "journal.h"
#include "entities.h"
#include "lock.h"
#include "customer_store.h"
#include "loan_store.h"
#include "rec_store.h"

#define JR_MAGIC  0x4A524E4C
#define JR_ACTIVE 1
#define JR_DONE   2

struct jr_rec   { int file; int id; int size; unsigned char old[JR_MAX]; };
struct jr_entry { int magic; int state; int n; struct jr_rec rec[JR_MAX_TARGETS]; unsigned int sum; };

static const char* jr_path(int f) {
    switch (f) {
    case JF_CUSTOMERS: return CUSTOMERS_FILE;
    case JF_LOANS:     return LOANS_FILE;
    case JF_EMPLOYEES: return EMPLOYEES_FILE;
    case JF_MANAGERS:  return MANAGERS_FILE;
    }
    return NULL;
}

static int jr_size(int f) {
    switch (f) {
    case JF_CUSTOMERS: return (int)sizeof(struct Customer);
    case JF_LOANS:     return (int)sizeof(struct Loan);
    case JF_EMPLOYEES: return (int)sizeof(struct Employee);
    case JF_MANAGERS:  return (int)sizeof(struct Manager);
    }
    return -1;
}

/* Checksum over the whole entry except `state` and `sum` (FNV-1a).
   A half-written entry (crash while writing it) fails this check. */
static unsigned int jr_sum(const struct jr_entry* e) {
    struct jr_entry c;
    memcpy(&c, e, sizeof(c));
    c.state = 0;
    c.sum = 0;
    const unsigned char* p = (const unsigned char*)&c;
    unsigned int h = 2166136261u;
    for (size_t i = 0; i < sizeof(c); i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

void crash_point(const char* name) {
    const char* want = getenv("BANK_CRASH");
    if (want != NULL && strcmp(want, name) == 0) {
        fprintf(stderr, "BANK_CRASH: simulated crash at '%s'\n", name);
        _exit(99);                       /* like a power cut: no cleanup at all */
    }
}

int jr_begin(const struct jr_target* t, int n) {
    if (n < 1 || n > JR_MAX_TARGETS) {
        return -1;
    }
    struct jr_entry e;
    memset(&e, 0, sizeof(e));
    e.magic = JR_MAGIC;
    e.state = JR_ACTIVE;
    e.n = n;
    for (int i = 0; i < n; i++) {
        const char* path = jr_path(t[i].file);
        int size = jr_size(t[i].file);
        if (path == NULL || size <= 0 || size > JR_MAX || t[i].id <= 0) {
            return -1;
        }
        e.rec[i].file = t[i].file;
        e.rec[i].id = t[i].id;
        e.rec[i].size = size;
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            return -1;
        }
        /* short read = record did not exist yet (e.g. new manager): old stays zeros */
        ssize_t r = pread(fd, e.rec[i].old, size, (off_t)(t[i].id - 1) * size);
        close(fd);
        if (r < 0) {
            return -1;
        }
    }
    e.sum = jr_sum(&e);

    /* journal.bin is a leaf lock: held only while picking a slot and writing */
    int jfd = open(JOURNAL_FILE, O_RDWR | O_CREAT, 0644);
    if (jfd < 0) {
        return -1;
    }
    if (lock_file(jfd, F_WRLCK) < 0) {
        close(jfd);
        return -1;
    }
    struct jr_entry cur;
    int slot = 0;
    while (pread(jfd, &cur, sizeof(cur), (off_t)slot * sizeof(cur)) == (ssize_t)sizeof(cur) &&
           cur.state == JR_ACTIVE) {
        slot++;                          /* skip slots of operations still running */
    }
    int ok = pwrite(jfd, &e, sizeof(e), (off_t)slot * sizeof(e)) == (ssize_t)sizeof(e) &&
             fsync(jfd) == 0;            /* journal on disk BEFORE any data changes */
    unlock_file(jfd);
    close(jfd);
    return ok ? slot : -1;
}

void jr_end(int slot) {
    if (slot < 0) {
        return;
    }
    int jfd = open(JOURNAL_FILE, O_RDWR);
    if (jfd < 0) {
        return;
    }
    struct jr_entry e;
    off_t off = (off_t)slot * sizeof(e);
    if (pread(jfd, &e, sizeof(e), off) == (ssize_t)sizeof(e)) {
        for (int i = 0; i < e.n && i < JR_MAX_TARGETS; i++) {
            const char* path = jr_path(e.rec[i].file);
            int fd = (path != NULL) ? open(path, O_RDONLY) : -1;
            if (fd >= 0) {
                fsync(fd);               /* data on disk BEFORE marking DONE */
                close(fd);
            }
        }
        int done = JR_DONE;
        if (pwrite(jfd, &done, sizeof(done), off + (off_t)offsetof(struct jr_entry, state)) ==
            (ssize_t)sizeof(done)) {
            fsync(jfd);
        }
    }
    close(jfd);
}

int jr_recover(void) {
    int jfd = open(JOURNAL_FILE, O_RDWR);
    if (jfd < 0) {
        return (errno == ENOENT) ? 0 : -1;
    }
    struct jr_entry e;
    int fixed = 0;
    off_t off = 0;
    while (pread(jfd, &e, sizeof(e), off) == (ssize_t)sizeof(e)) {
        off += sizeof(e);
        if (e.magic != JR_MAGIC || e.state != JR_ACTIVE || e.n < 1 ||
            e.n > JR_MAX_TARGETS || e.sum != jr_sum(&e)) {
            continue;                    /* finished, empty or half-written entry */
        }
        for (int i = e.n - 1; i >= 0; i--) {
            const char* path = jr_path(e.rec[i].file);
            int size = jr_size(e.rec[i].file);
            if (path == NULL || size != e.rec[i].size) {
                continue;
            }
            int fd = open(path, O_RDWR);
            if (fd < 0 ||
                pwrite(fd, e.rec[i].old, size, (off_t)(e.rec[i].id - 1) * size) != (ssize_t)size ||
                fsync(fd) != 0) {
                if (fd >= 0) {
                    close(fd);
                }
                close(jfd);
                return -1;
            }
            close(fd);
        }
        fixed++;
    }
    if (ftruncate(jfd, 0) != 0 || fsync(jfd) != 0) {    /* all handled: empty journal */
        close(jfd);
        return -1;
    }
    close(jfd);
    return fixed;
}
