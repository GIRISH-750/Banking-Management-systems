#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include "manager_ops.h"
#include "customer_store.h"
#include "rec_store.h"
#include "counter.h"
#include "lock.h"

static void set_str(char* dst, const char* src, size_t len) {
    strncpy(dst, src, len - 1);
    dst[len - 1] = '\0';
}

/* Write lock on the customer: waits for any running apply/transfer on him. */
int mgr_set_customer_status(int id, int activate) {
    if (id <= 0) {
        return MG_NOT_FOUND;
    }
    int fd = customer_open();
    if (fd < 0) {
        return MG_ERROR;
    }
    struct Customer c;
    if (lock_record(fd, id, sizeof(c), F_WRLCK) < 0) {
        close(fd);
        return MG_ERROR;
    }
    const char* want = activate ? ST_ACTIVE : ST_INACTIVE;
    int rc;
    int s = customer_read(fd, id, &c);
    if (s == STORE_ERROR) {
        rc = MG_ERROR;
    } else if (s == STORE_NOT_FOUND || c.userID != id) {
        rc = MG_NOT_FOUND;
    } else if (strcmp(c.status, ST_DELETED) == 0) {
        rc = MG_DELETED;
    } else if (strcmp(c.status, want) == 0) {
        rc = MG_ALREADY;
    } else {
        set_str(c.status, want, STATUS_LEN);
        rc = (customer_write(fd, id, &c) == STORE_OK) ? MG_OK : MG_ERROR;
    }
    unlock_record(fd, id, sizeof(c));
    close(fd);
    return rc;
}

/* New ID from counter.bin -> a record lock on that ID is enough. */
int fb_submit(int cust_id, const char* msg, int* fb_id) {
    size_t n = strlen(msg);
    if (n == 0 || n >= MSG_LEN) {
        return MG_BAD_MSG;
    }
    int fd = open(FEEDBACK_FILE, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        return MG_ERROR;
    }
    int id = counter_next(offsetof(struct Counter, next_feedback_id));
    struct Feedback f;
    if (id < 0 || lock_record(fd, id, sizeof(f), F_WRLCK) < 0) {
        close(fd);
        return MG_ERROR;
    }
    memset(&f, 0, sizeof(f));
    f.feedbackID = id;
    f.customerID = cust_id;
    set_str(f.message, msg, MSG_LEN);
    set_str(f.status, FB_NEW, STATUS_LEN);
    int rc = (rec_write(fd, id, &f, sizeof(f)) == REC_OK) ? MG_OK : MG_ERROR;
    unlock_record(fd, id, sizeof(f));
    close(fd);
    if (rc == MG_OK) {
        *fb_id = id;
    }
    return rc;
}

int fb_list(struct Feedback* out, int max) {
    int fd = open(FEEDBACK_FILE, O_RDONLY);
    if (fd < 0) {
        return (errno == ENOENT) ? 0 : -1;
    }
    if (lock_file(fd, F_RDLCK) < 0) {
        close(fd);
        return -1;
    }
    struct Feedback f;
    int count = 0;
    off_t off = 0;
    ssize_t r;
    while (count < max && (r = pread(fd, &f, sizeof(f), off)) == (ssize_t)sizeof(f)) {
        off += sizeof(f);
        if (f.feedbackID > 0) {
            out[count++] = f;
        }
    }
    unlock_file(fd);
    close(fd);
    return count;
}

int fb_review(int fb_id) {
    if (fb_id <= 0) {
        return MG_NOT_FOUND;
    }
    int fd = open(FEEDBACK_FILE, O_RDWR);
    if (fd < 0) {
        return (errno == ENOENT) ? MG_NOT_FOUND : MG_ERROR;
    }
    struct Feedback f;
    if (lock_record(fd, fb_id, sizeof(f), F_WRLCK) < 0) {
        close(fd);
        return MG_ERROR;
    }
    int rc;
    int s = rec_read(fd, fb_id, &f, sizeof(f));
    if (s == REC_ERROR) {
        rc = MG_ERROR;
    } else if (s == REC_NOT_FOUND || f.feedbackID != fb_id) {
        rc = MG_NOT_FOUND;
    } else if (strcmp(f.status, FB_NEW) != 0) {
        rc = MG_BAD_STATE;
    } else {
        set_str(f.status, FB_REVIEWED, STATUS_LEN);
        rc = (rec_write(fd, fb_id, &f, sizeof(f)) == REC_OK) ? MG_OK : MG_ERROR;
    }
    unlock_record(fd, fb_id, sizeof(f));
    close(fd);
    return rc;
}

const char* mgr_message(int rc) {
    switch (rc) {
    case MG_OK:        return "OK";
    case MG_NOT_FOUND: return "ERR Not found";
    case MG_DELETED:   return "ERR Account is deleted (cannot be changed)";
    case MG_ALREADY:   return "ERR Account is already in that status";
    case MG_BAD_MSG:   return "ERR Feedback must be 1-199 characters";
    case MG_BAD_STATE: return "ERR Feedback already reviewed";
    default:           return "ERR Internal error";
    }
}
