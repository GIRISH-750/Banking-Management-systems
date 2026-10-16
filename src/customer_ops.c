#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "customer_ops.h"
#include "customer_store.h"
#include "lock.h"

#define MAX_AMOUNT 1000000.0f   /* refuse absurd amounts in one go */

static int is_active(const struct Customer* c) {
    return strcmp(c->status, "ACTIVE") == 0;
}

int op_balance(int id, float* balance) {
    struct Customer c;
    int fd = customer_open();
    if (fd < 0) {
        return OP_ERROR;
    }

    lock_record(fd, id, sizeof(c), F_RDLCK);        /* shared: readers don't block each other */
    int rc = customer_read(fd, id, &c);
    unlock_record(fd, id, sizeof(c));
    close(fd);

    if (rc == STORE_NOT_FOUND) {
        return OP_NOT_FOUND;
    }
    if (rc != STORE_OK) {
        return OP_ERROR;
    }
    *balance = c.balance;
    return OP_OK;
}

/* Shared body of deposit and withdraw. sign = +1 or -1. */
static int change_balance(int id, float amount, int sign, float* balance) {
    if (!(amount > 0.0f) || amount > MAX_AMOUNT) {   /* also rejects NaN */
        return OP_BAD_AMOUNT;
    }

    struct Customer c;
    int fd = customer_open();
    if (fd < 0) {
        return OP_ERROR;
    }

    lock_record(fd, id, sizeof(c), F_WRLCK);        /* exclusive: whole read-change-write */
    int result;
    int rc = customer_read(fd, id, &c);

    if (rc == STORE_NOT_FOUND) {
        result = OP_NOT_FOUND;
    } else if (rc != STORE_OK) {
        result = OP_ERROR;
    } else if (!is_active(&c)) {
        result = OP_INACTIVE;
    } else if (sign < 0 && c.balance < amount) {
        result = OP_NO_FUNDS;
    } else {
        c.balance += sign * amount;
        if (customer_write(fd, id, &c) != STORE_OK) {
            result = OP_ERROR;
        } else {
            *balance = c.balance;
            result = OP_OK;
        }
    }

    unlock_record(fd, id, sizeof(c));                /* always unlock, on every path */
    close(fd);
    return result;
}

int op_deposit(int id, float amount, float* balance) {
    return change_balance(id, amount, +1, balance);
}

int op_withdraw(int id, float amount, float* balance) {
    return change_balance(id, amount, -1, balance);
}

const char* op_message(int rc) {
    switch (rc) {
        case OP_OK:         return "OK";
        case OP_NOT_FOUND:  return "ERR Account not found";
        case OP_INACTIVE:   return "ERR Account is inactive";
        case OP_BAD_AMOUNT: return "ERR Invalid amount";
        case OP_NO_FUNDS:   return "ERR Insufficient balance";
        default:            return "ERR Internal error";
    }
}
