#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include "employee_ops.h"
#include "customer_store.h"
#include "lock.h"
#include "counter.h"
#include "txn_store.h"
#include "hash.h"

static int valid_name(const char* s) {
    size_t n = strlen(s);
    return n > 0 && n < NAME_LEN;
}

int emp_add_customer(const char* first, const char* last,
                     const char* password, int* new_id) {
    if (!valid_name(first) || !valid_name(last) || strlen(password) == 0) {
        return EMP_BAD_INPUT;
    }
    int id = counter_next(offsetof(struct Counter, next_customer_id));
    if (id < 0) {
        return EMP_ERROR;
    }
    struct Customer c;
    memset(&c, 0, sizeof(c));
    c.userID = id;
    strncpy(c.firstName, first, NAME_LEN - 1);
    strncpy(c.lastName, last, NAME_LEN - 1);
    hash_password(password, c.password);
    strncpy(c.status, ST_ACTIVE, STATUS_LEN - 1);

    int fd = customer_open();
    if (fd < 0) {
        return EMP_ERROR;
    }
    int rc = EMP_ERROR;
    if (lock_record(fd, id, sizeof(c), F_WRLCK) == 0) {
        if (customer_write(fd, id, &c) == STORE_OK) {
            *new_id = id;
            rc = EMP_OK;
        }
        unlock_record(fd, id, sizeof(c));
    }
    close(fd);
    return rc;
}

int emp_modify_customer(int id, const char* first, const char* last) {
    if (id <= 0 || !valid_name(first) || !valid_name(last)) {
        return EMP_BAD_INPUT;
    }
    int fd = customer_open();
    if (fd < 0) {
        return EMP_ERROR;
    }
    struct Customer c;
    if (lock_record(fd, id, sizeof(c), F_WRLCK) < 0) {
        close(fd);
        return EMP_ERROR;
    }
    int rc;
    int s = customer_read(fd, id, &c);
    if (s == STORE_NOT_FOUND || (s == STORE_OK && c.userID != id)) {
        rc = EMP_NOT_FOUND;
    } else if (s != STORE_OK) {
        rc = EMP_ERROR;
    } else {
        memset(c.firstName, 0, NAME_LEN);
        memset(c.lastName, 0, NAME_LEN);
        strncpy(c.firstName, first, NAME_LEN - 1);
        strncpy(c.lastName, last, NAME_LEN - 1);
        rc = (customer_write(fd, id, &c) == STORE_OK) ? EMP_OK : EMP_ERROR;
    }
    unlock_record(fd, id, sizeof(c));
    close(fd);
    return rc;
}

int emp_passbook(int id, struct Transaction* out, int max, int* n) {
    if (id <= 0) {
        return EMP_BAD_INPUT;
    }
    int fd = customer_open();
    if (fd < 0) {
        return EMP_ERROR;
    }
    struct Customer c;
    if (lock_record(fd, id, sizeof(c), F_RDLCK) < 0) {
        close(fd);
        return EMP_ERROR;
    }
    int s = customer_read(fd, id, &c);
    unlock_record(fd, id, sizeof(c));
    close(fd);
    if (s == STORE_NOT_FOUND || (s == STORE_OK && c.userID != id)) {
        return EMP_NOT_FOUND;
    }
    if (s != STORE_OK) {
        return EMP_ERROR;
    }
    int k = txn_history(id, out, max);
    if (k < 0) {
        return EMP_ERROR;
    }
    *n = k;
    return EMP_OK;
}

const char* emp_message(int rc) {
    switch (rc) {
        case EMP_NOT_FOUND: return "ERR No such customer";
        case EMP_BAD_INPUT: return "ERR Invalid input";
        default:            return "ERR Internal error";
    }
}
