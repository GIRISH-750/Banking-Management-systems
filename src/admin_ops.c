#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include "admin_ops.h"
#include "rec_store.h"
#include "customer_store.h"
#include "loan_store.h"
#include "lock.h"
#include "counter.h"
#include "hash.h"

static int valid_name(const char* s) {
    size_t n = strlen(s);
    return n > 0 && n < NAME_LEN;
}

static void set_str(char* dst, const char* src, size_t len) {
    memset(dst, 0, len);
    strncpy(dst, src, len - 1);
}

/* Opens employees.bin, WRITE-locks record id, reads it (must exist, not DELETED).
   On ADM_OK the caller must call emp_release() later. */
static int emp_acquire(int id, int* fd, struct Employee* e) {
    if (id <= 0) {
        return ADM_BAD_INPUT;
    }
    *fd = rec_open(EMPLOYEES_FILE);
    if (*fd < 0) {
        return ADM_ERROR;
    }
    if (lock_record(*fd, id, sizeof(*e), F_WRLCK) < 0) {
        close(*fd);
        return ADM_ERROR;
    }
    int s = rec_read(*fd, id, e, sizeof(*e));
    if (s == REC_OK && e->employeeID == id && strcmp(e->status, ST_DELETED) != 0) {
        return ADM_OK;
    }
    unlock_record(*fd, id, sizeof(*e));
    close(*fd);
    return (s == REC_ERROR) ? ADM_ERROR : ADM_NOT_FOUND;
}

static void emp_release(int fd, int id) {
    unlock_record(fd, id, sizeof(struct Employee));
    close(fd);
}

int adm_add_employee(const char* first, const char* last, const char* pw, int* new_id) {
    if (!valid_name(first) || !valid_name(last) || strlen(pw) < 4) {
        return ADM_BAD_INPUT;
    }
    int id = counter_next(offsetof(struct Counter, next_employee_id));
    if (id < 0) {
        return ADM_ERROR;
    }
    struct Employee e;
    memset(&e, 0, sizeof(e));
    e.employeeID = id;
    set_str(e.first_name, first, NAME_LEN);
    set_str(e.last_name, last, NAME_LEN);
    hash_password(pw, e.password);
    set_str(e.status, ST_ACTIVE, STATUS_LEN);

    int fd = rec_open(EMPLOYEES_FILE);
    if (fd < 0) {
        return ADM_ERROR;
    }
    int rc = ADM_ERROR;
    if (lock_record(fd, id, sizeof(e), F_WRLCK) == 0) {
        if (rec_write(fd, id, &e, sizeof(e)) == REC_OK) {
            *new_id = id;
            rc = ADM_OK;
        }
        unlock_record(fd, id, sizeof(e));
    }
    close(fd);
    return rc;
}

int adm_modify_employee(int id, const char* first, const char* last) {
    if (!valid_name(first) || !valid_name(last)) {
        return ADM_BAD_INPUT;
    }
    int fd;
    struct Employee e;
    int rc = emp_acquire(id, &fd, &e);
    if (rc != ADM_OK) {
        return rc;
    }
    set_str(e.first_name, first, NAME_LEN);
    set_str(e.last_name, last, NAME_LEN);
    rc = (rec_write(fd, id, &e, sizeof(e)) == REC_OK) ? ADM_OK : ADM_ERROR;
    emp_release(fd, id);
    return rc;
}

/* Soft delete: status = DELETED. Blocked while loans are ASSIGNED to him. */
int adm_delete_employee(int id) {
    int fd;
    struct Employee e;
    int rc = emp_acquire(id, &fd, &e);
    if (rc != ADM_OK) {
        return rc;
    }
    int n = loans_assigned_to(id);
    if (n < 0) {
        rc = ADM_ERROR;
    } else if (n > 0) {
        rc = ADM_HAS_LOANS;
    } else {
        set_str(e.status, ST_DELETED, STATUS_LEN);
        rc = (rec_write(fd, id, &e, sizeof(e)) == REC_OK) ? ADM_OK : ADM_ERROR;
    }
    emp_release(fd, id);
    return rc;
}

/* Soft delete: balance must be 0 and no open loan. */
int adm_delete_customer(int id) {
    if (id <= 0) {
        return ADM_BAD_INPUT;
    }
    int fd = customer_open();
    if (fd < 0) {
        return ADM_ERROR;
    }
    struct Customer c;
    if (lock_record(fd, id, sizeof(c), F_WRLCK) < 0) {
        close(fd);
        return ADM_ERROR;
    }
    int rc;
    int s = customer_read(fd, id, &c);
    if (s == STORE_NOT_FOUND || (s == STORE_OK &&
        (c.userID != id || strcmp(c.status, ST_DELETED) == 0))) {
        rc = ADM_NOT_FOUND;
    } else if (s != STORE_OK) {
        rc = ADM_ERROR;
    } else if (c.balance > 0.005f || c.balance < -0.005f) {
        rc = ADM_HAS_BALANCE;
    } else {
        int n = loans_open_of_customer(id);
        if (n < 0) {
            rc = ADM_ERROR;
        } else if (n > 0) {
            rc = ADM_HAS_LOANS;
        } else {
            set_str(c.status, ST_DELETED, STATUS_LEN);
            rc = (customer_write(fd, id, &c) == STORE_OK) ? ADM_OK : ADM_ERROR;
        }
    }
    unlock_record(fd, id, sizeof(c));
    close(fd);
    return rc;
}

int adm_view_customer(int id, struct Customer* out) {
    if (id <= 0) {
        return ADM_BAD_INPUT;
    }
    int fd = customer_open();
    if (fd < 0) {
        return ADM_ERROR;
    }
    if (lock_record(fd, id, sizeof(*out), F_RDLCK) < 0) {
        close(fd);
        return ADM_ERROR;
    }
    int s = customer_read(fd, id, out);
    unlock_record(fd, id, sizeof(*out));
    close(fd);
    if (s == STORE_NOT_FOUND || (s == STORE_OK && out->userID != id)) {
        return ADM_NOT_FOUND;
    }
    return (s == STORE_OK) ? ADM_OK : ADM_ERROR;
}

/* Promote: 1) create manager record  2) set employee INACTIVE.
   If 2 fails, roll back 1. Employee record stays write-locked throughout,
   so nobody can assign him a loan in between. */
int adm_promote(int emp_id, int* new_mgr_id) {
    int efd;
    struct Employee e;
    int rc = emp_acquire(emp_id, &efd, &e);
    if (rc != ADM_OK) {
        return rc;
    }
    if (strcmp(e.status, ST_ACTIVE) != 0) {
        emp_release(efd, emp_id);
        return ADM_NOT_ACTIVE;
    }
    int n = loans_assigned_to(emp_id);
    if (n != 0) {
        emp_release(efd, emp_id);
        return (n < 0) ? ADM_ERROR : ADM_HAS_LOANS;
    }

    /* Step 1: create the manager record (same name + password hash) */
    int mid = counter_next(offsetof(struct Counter, next_manager_id));
    int mfd = (mid > 0) ? rec_open(MANAGERS_FILE) : -1;
    if (mfd < 0) {
        emp_release(efd, emp_id);
        return ADM_ERROR;
    }
    struct Manager m;
    memset(&m, 0, sizeof(m));
    m.managerID = mid;
    set_str(m.first_name, e.first_name, NAME_LEN);
    set_str(m.last_name, e.last_name, NAME_LEN);
    memcpy(m.password, e.password, PASS_LEN);
    set_str(m.status, ST_ACTIVE, STATUS_LEN);
    if (lock_record(mfd, mid, sizeof(m), F_WRLCK) < 0 ||
        rec_write(mfd, mid, &m, sizeof(m)) != REC_OK) {
        close(mfd);                      /* closing the fd also drops its OFD lock */
        emp_release(efd, emp_id);
        return ADM_ERROR;
    }

    /* Step 2: retire the employee record */
    set_str(e.status, ST_INACTIVE, STATUS_LEN);
    if (rec_write(efd, emp_id, &e, sizeof(e)) != REC_OK) {
        set_str(m.status, ST_DELETED, STATUS_LEN);    /* rollback step 1 */
        rec_write(mfd, mid, &m, sizeof(m));
        close(mfd);
        emp_release(efd, emp_id);
        return ADM_ERROR;
    }
    close(mfd);
    emp_release(efd, emp_id);
    *new_mgr_id = mid;
    return ADM_OK;
}

/* Demote: same 2 steps the other way round. Whole-file lock on managers.bin,
   so "count active managers" + "demote one" happen as one atomic step
   (two admins can't demote the last two managers at the same time). */
int adm_demote(int mgr_id, int* new_emp_id) {
    if (mgr_id <= 0) {
        return ADM_BAD_INPUT;
    }
    int mfd = rec_open(MANAGERS_FILE);
    if (mfd < 0) {
        return ADM_ERROR;
    }
    if (lock_file(mfd, F_WRLCK) < 0) {
        close(mfd);
        return ADM_ERROR;
    }
    struct Manager m, t;
    int rc = ADM_OK;
    int s = rec_read(mfd, mgr_id, &m, sizeof(m));
    if (s == REC_ERROR) {
        rc = ADM_ERROR;
    } else if (s == REC_NOT_FOUND || m.managerID != mgr_id) {
        rc = ADM_NOT_FOUND;
    } else if (strcmp(m.status, ST_ACTIVE) != 0) {
        rc = ADM_NOT_ACTIVE;
    } else {
        int active = 0;
        for (int i = 1; rec_read(mfd, i, &t, sizeof(t)) == REC_OK; i++) {
            if (strcmp(t.status, ST_ACTIVE) == 0) {
                active++;
            }
        }
        if (active <= 1) {
            rc = ADM_LAST_MANAGER;
        }
    }
    if (rc != ADM_OK) {
        unlock_file(mfd);
        close(mfd);
        return rc;
    }

    /* Step 1: create the employee record */
    int eid = counter_next(offsetof(struct Counter, next_employee_id));
    int efd = (eid > 0) ? rec_open(EMPLOYEES_FILE) : -1;
    struct Employee e;
    memset(&e, 0, sizeof(e));
    e.employeeID = eid;
    set_str(e.first_name, m.first_name, NAME_LEN);
    set_str(e.last_name, m.last_name, NAME_LEN);
    memcpy(e.password, m.password, PASS_LEN);
    set_str(e.status, ST_ACTIVE, STATUS_LEN);
    if (efd < 0 || lock_record(efd, eid, sizeof(e), F_WRLCK) < 0 ||
        rec_write(efd, eid, &e, sizeof(e)) != REC_OK) {
        if (efd >= 0) {
            close(efd);
        }
        unlock_file(mfd);
        close(mfd);
        return ADM_ERROR;
    }

    /* Step 2: retire the manager record */
    set_str(m.status, ST_INACTIVE, STATUS_LEN);
    if (rec_write(mfd, mgr_id, &m, sizeof(m)) != REC_OK) {
        set_str(e.status, ST_DELETED, STATUS_LEN);    /* rollback step 1 */
        rec_write(efd, eid, &e, sizeof(e));
        close(efd);
        unlock_file(mfd);
        close(mfd);
        return ADM_ERROR;
    }
    close(efd);
    unlock_file(mfd);
    close(mfd);
    *new_emp_id = eid;
    return ADM_OK;
}

const char* adm_message(int rc) {
    switch (rc) {
        case ADM_NOT_FOUND:    return "ERR No such user";
        case ADM_BAD_INPUT:    return "ERR Invalid input";
        case ADM_NOT_ACTIVE:   return "ERR User is not active";
        case ADM_HAS_LOANS:    return "ERR User has open loans";
        case ADM_HAS_BALANCE:  return "ERR Balance must be 0 before closing";
        case ADM_LAST_MANAGER: return "ERR Cannot demote the last active manager";
        default:               return "ERR Internal error";
    }
}
