#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include "loan_ops.h"
#include "loan_store.h"
#include "customer_store.h"
#include "rec_store.h"
#include "counter.h"
#include "lock.h"
#include "txn_store.h"

/* LOCK ORDER (whole project): employees -> loans -> customers.
   counter.bin is a leaf lock: taken last, released at once. */

enum { SCAN_OPEN_OF_CUST, SCAN_ALL_OF_CUST, SCAN_PENDING, SCAN_ASSIGNED_TO, SCAN_ALL };

static void set_str(char* dst, const char* src, size_t len) {
    strncpy(dst, src, len - 1);
    dst[len - 1] = '\0';
}

static int matches(const struct Loan* l, int mode, int id) {
    if (l->loanID <= 0) {
        return 0;                       /* empty slot (zeros) */
    }
    int pending  = strcmp(l->status, LOAN_PENDING) == 0;
    int assigned = strcmp(l->status, LOAN_ASSIGNED) == 0;
    switch (mode) {
    case SCAN_OPEN_OF_CUST: return l->userID == id && (pending || assigned);
    case SCAN_ALL_OF_CUST:  return l->userID == id;
    case SCAN_PENDING:      return pending;
    case SCAN_ASSIGNED_TO:  return assigned && l->assignedEmpID == id;
    case SCAN_ALL:          return 1;
    }
    return 0;
}

/* Caller must already hold a lock on loans.bin. Returns total matches
   (copies up to max into out, if out != NULL), or -1 on error. */
static int scan_loans(int fd, int mode, int id, struct Loan* out, int max) {
    struct Loan l;
    int count = 0;
    off_t off = 0;
    ssize_t n;
    while ((n = pread(fd, &l, sizeof(l), off)) == (ssize_t)sizeof(l)) {
        off += sizeof(l);
        if (matches(&l, mode, id)) {
            if (out != NULL && count < max) {
                out[count] = l;
            }
            count++;
        }
    }
    return (n < 0) ? -1 : count;
}

/* Both locks already held: loans.bin (whole file, write), customer (read). */
static int apply_locked(int lfd, int cfd, int cust_id, float amount, int* loan_id) {
    struct Customer c;
    int s = customer_read(cfd, cust_id, &c);
    if (s == STORE_ERROR) {
        return LN_ERROR;
    }
    if (s == STORE_NOT_FOUND || c.userID != cust_id || strcmp(c.status, ST_DELETED) == 0) {
        return LN_NOT_FOUND;
    }
    if (strcmp(c.status, ST_ACTIVE) != 0) {
        return LN_INACTIVE;
    }
    int open_n = scan_loans(lfd, SCAN_OPEN_OF_CUST, cust_id, NULL, 0);
    if (open_n != 0) {
        return (open_n < 0) ? LN_ERROR : LN_HAS_OPEN;
    }
    int id = counter_next(offsetof(struct Counter, next_loan_id));
    if (id < 0) {
        return LN_ERROR;
    }
    struct Loan l;
    memset(&l, 0, sizeof(l));
    l.loanID = id;
    l.userID = cust_id;
    l.amount = amount;
    set_str(l.status, LOAN_PENDING, STATUS_LEN);
    l.assignedEmpID = 0;
    if (rec_write(lfd, id, &l, sizeof(l)) != REC_OK) {
        return LN_ERROR;
    }
    *loan_id = id;
    return LN_OK;
}

int loan_apply(int cust_id, float amount, int* loan_id) {
    if (!(amount > 0.0f) || amount > LOAN_MAX) {     /* also rejects NaN */
        return LN_BAD_AMOUNT;
    }
    if (cust_id <= 0) {
        return LN_NOT_FOUND;
    }
    int lfd = open(LOANS_FILE, O_RDWR | O_CREAT, 0644);
    if (lfd < 0) {
        return LN_ERROR;
    }
    if (lock_file(lfd, F_WRLCK) < 0) {           /* 1st: loans.bin */
        close(lfd);
        return LN_ERROR;
    }
    int rc = LN_ERROR;
    int cfd = customer_open();
    if (cfd >= 0) {
        if (lock_record(cfd, cust_id, sizeof(struct Customer), F_RDLCK) == 0) {
            rc = apply_locked(lfd, cfd, cust_id, amount, loan_id);   /* 2nd: customer */
            unlock_record(cfd, cust_id, sizeof(struct Customer));
        }
        close(cfd);
    }
    unlock_file(lfd);
    close(lfd);
    return rc;
}

static int list_loans(int mode, int id, struct Loan* out, int max) {
    int fd = open(LOANS_FILE, O_RDONLY);
    if (fd < 0) {
        return (errno == ENOENT) ? 0 : -1;      /* no file yet = no loans */
    }
    if (lock_file(fd, F_RDLCK) < 0) {
        close(fd);
        return -1;
    }
    int n = scan_loans(fd, mode, id, out, max);
    unlock_file(fd);
    close(fd);
    return (n > max) ? max : n;
}

int loan_list_customer(int cust_id, struct Loan* out, int max) {
    return list_loans(SCAN_ALL_OF_CUST, cust_id, out, max);
}
int loan_list_pending(struct Loan* out, int max) {
    return list_loans(SCAN_PENDING, 0, out, max);
}
int loan_list_assigned(int emp_id, struct Loan* out, int max) {
    return list_loans(SCAN_ASSIGNED_TO, emp_id, out, max);
}

const char* loan_message(int rc) {
    switch (rc) {
    case LN_OK:           return "OK";
    case LN_NOT_FOUND:    return "ERR Not found";
    case LN_INACTIVE:     return "ERR Account is inactive";
    case LN_BAD_AMOUNT:   return "ERR Invalid amount (must be > 0 and <= 1000000)";
    case LN_HAS_OPEN:     return "ERR You already have an open loan (PENDING or ASSIGNED)";
    case LN_BAD_STATE:    return "ERR Loan is not in the right status for this action";
    case LN_NOT_YOURS:    return "ERR This loan is not assigned to you";
    case LN_EMP_INACTIVE: return "ERR Employee is not active";
    case LN_EMP_NOT_FOUND: return "ERR Employee not found";
    default:              return "ERR Internal error";
    }
}

/* Employee read lock already held by the caller. Locks only this loan's record. */
static int assign_locked(int loan_id, int emp_id) {
    int lfd = open(LOANS_FILE, O_RDWR);
    if (lfd < 0) {
        return (errno == ENOENT) ? LN_NOT_FOUND : LN_ERROR;
    }
    struct Loan l;
    if (lock_record(lfd, loan_id, sizeof(l), F_WRLCK) < 0) {
        close(lfd);
        return LN_ERROR;
    }
    int rc;
    int s = rec_read(lfd, loan_id, &l, sizeof(l));
    if (s == REC_ERROR) {
        rc = LN_ERROR;
    } else if (s == REC_NOT_FOUND || l.loanID != loan_id) {
        rc = LN_NOT_FOUND;
    } else if (strcmp(l.status, LOAN_PENDING) != 0) {
        rc = LN_BAD_STATE;               /* already assigned / decided */
    } else {
        set_str(l.status, LOAN_ASSIGNED, STATUS_LEN);
        l.assignedEmpID = emp_id;
        rc = (rec_write(lfd, loan_id, &l, sizeof(l)) == REC_OK) ? LN_OK : LN_ERROR;
    }
    unlock_record(lfd, loan_id, sizeof(l));
    close(lfd);
    return rc;
}

/* Lock order: employee (READ) -> loan record (WRITE).
   The employee read lock pairs with promote/delete employee, which hold the
   employee WRITE lock while they check "no assigned loans" and retire him. */
int loan_assign(int loan_id, int emp_id) {
    if (loan_id <= 0) {
        return LN_NOT_FOUND;
    }
    if (emp_id <= 0) {
        return LN_EMP_NOT_FOUND;
    }
    int efd = rec_open(EMPLOYEES_FILE);
    if (efd < 0) {
        return LN_ERROR;
    }
    struct Employee e;
    if (lock_record(efd, emp_id, sizeof(e), F_RDLCK) < 0) {
        close(efd);
        return LN_ERROR;
    }
    int rc;
    int s = rec_read(efd, emp_id, &e, sizeof(e));
    if (s == REC_ERROR) {
        rc = LN_ERROR;
    } else if (s == REC_NOT_FOUND || e.employeeID != emp_id || strcmp(e.status, ST_DELETED) == 0) {
        rc = LN_EMP_NOT_FOUND;
    } else if (strcmp(e.status, ST_ACTIVE) != 0) {
        rc = LN_EMP_INACTIVE;            /* e.g. promoted to manager */
    } else {
        rc = assign_locked(loan_id, emp_id);
    }
    unlock_record(efd, emp_id, sizeof(e));
    close(efd);
    return rc;
}

/* Manager: every loan, any status (supervisor view). Read-only. */
int loan_list_all(struct Loan* out, int max) {
    return list_loans(SCAN_ALL, 0, out, max);
}

/* Both locks held (loan + customer). Loan first, then money; undo on failure. */
static int credit_customer(int lfd, int cfd, struct Loan* l, struct Customer* c) {
    set_str(l->status, LOAN_APPROVED, STATUS_LEN);
    if (rec_write(lfd, l->loanID, l, sizeof(*l)) != REC_OK) {
        return LN_ERROR;                          /* nothing changed */
    }
    c->balance += l->amount;
    c->loan += l->amount;
    if (customer_write(cfd, l->userID, c) != STORE_OK) {
        set_str(l->status, LOAN_ASSIGNED, STATUS_LEN);   /* undo step 1 */
        rec_write(lfd, l->loanID, l, sizeof(*l));
        return LN_ERROR;
    }
    txn_log(l->userID, TXN_LOAN_CREDIT, l->amount);  /* while still locked */
    return LN_OK;
}

/* Loan write lock already held. Locks the customer, credits the money.
   Write order: loan APPROVED first, then the money. A crash in between
   leaves "approved but not paid" (no money created), never "paid but
   still ASSIGNED" (could be approved twice). Day 6 journal closes the gap. */
static int approve_locked(int lfd, struct Loan* l) {
    int cfd = customer_open();
    if (cfd < 0) {
        return LN_ERROR;
    }
    struct Customer c;
    if (lock_record(cfd, l->userID, sizeof(c), F_WRLCK) < 0) {
        close(cfd);
        return LN_ERROR;
    }
    int rc;
    int s = customer_read(cfd, l->userID, &c);
    if (s == STORE_ERROR) {
        rc = LN_ERROR;
    } else if (s == STORE_NOT_FOUND || c.userID != l->userID || strcmp(c.status, ST_DELETED) == 0) {
        rc = LN_NOT_FOUND;
    } else if (strcmp(c.status, ST_ACTIVE) != 0) {
        rc = LN_INACTIVE;                         /* loan stays ASSIGNED */
    } else {
        rc = credit_customer(lfd, cfd, l, &c);
    }
    unlock_record(cfd, l->userID, sizeof(c));
    close(cfd);
    return rc;
}

/* Employee: ASSIGNED -> APPROVED / REJECTED. Lock order: loan -> customer.
   No employee lock needed: promote/delete refuse while he has assigned loans. */
int loan_decide(int loan_id, int emp_id, int approve) {
    if (loan_id <= 0) {
        return LN_NOT_FOUND;
    }
    int lfd = open(LOANS_FILE, O_RDWR);
    if (lfd < 0) {
        return (errno == ENOENT) ? LN_NOT_FOUND : LN_ERROR;
    }
    struct Loan l;
    if (lock_record(lfd, loan_id, sizeof(l), F_WRLCK) < 0) {
        close(lfd);
        return LN_ERROR;
    }
    int rc;
    int s = rec_read(lfd, loan_id, &l, sizeof(l));
    if (s == REC_ERROR) {
        rc = LN_ERROR;
    } else if (s == REC_NOT_FOUND || l.loanID != loan_id) {
        rc = LN_NOT_FOUND;
    } else if (strcmp(l.status, LOAN_ASSIGNED) != 0) {
        rc = LN_BAD_STATE;                        /* blocks double approval */
    } else if (l.assignedEmpID != emp_id) {
        rc = LN_NOT_YOURS;
    } else if (!approve) {
        set_str(l.status, LOAN_REJECTED, STATUS_LEN);
        rc = (rec_write(lfd, loan_id, &l, sizeof(l)) == REC_OK) ? LN_OK : LN_ERROR;
    } else {
        rc = approve_locked(lfd, &l);
    }
    unlock_record(lfd, loan_id, sizeof(l));
    close(lfd);
    return rc;
}
