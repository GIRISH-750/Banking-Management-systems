#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "entities.h"
#include "loan_store.h"
#include "lock.h"

/* Scans loans.bin under a whole-file read lock. */
static int count_loans(int by_emp, int id) {
    int fd = open(LOANS_FILE, O_RDONLY);
    if (fd < 0) {
        return (errno == ENOENT) ? 0 : -1;   /* no loans file yet = no loans */
    }
    if (lock_file(fd, F_RDLCK) < 0) {
        close(fd);
        return -1;
    }
    struct Loan l;
    int count = 0;
    off_t off = 0;
    ssize_t n;
    while ((n = pread(fd, &l, sizeof(l), off)) == (ssize_t)sizeof(l)) {
        off += sizeof(l);
        if (by_emp) {
            if (l.assignedEmpID == id && strcmp(l.status, LOAN_ASSIGNED) == 0) {
                count++;
            }
        } else if (l.userID == id && (strcmp(l.status, LOAN_PENDING) == 0 ||
                                      strcmp(l.status, LOAN_ASSIGNED) == 0)) {
            count++;
        }
    }
    unlock_file(fd);
    close(fd);
    return (n < 0) ? -1 : count;
}

int loans_assigned_to(int emp_id) {
    return count_loans(1, emp_id);
}

int loans_open_of_customer(int cust_id) {
    return count_loans(0, cust_id);
}
