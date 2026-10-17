#include <stdio.h>
#include <string.h>
#include "customer_handler.h"
#include "customer_ops.h"
#include "net_util.h"
#include "config.h"

/* Reads "<CMD> <amount>" with nothing after the amount. Returns 1 on success. */
static int parse_amount(const char* line, const char* cmd, float* amount) {
    char fmt[32], extra;
    snprintf(fmt, sizeof(fmt), "%s %%f %%c", cmd);
    return sscanf(line, fmt, amount, &extra) == 1;
}

static void reply(int fd, int rc, float balance) {
    char msg[MAX_LINE_LEN];
    if (rc == OP_OK) {
        snprintf(msg, sizeof(msg), "OK Balance: %.2f\n", balance);
    } else {
        snprintf(msg, sizeof(msg), "%s\n", op_message(rc));
    }
    send_line(fd, msg);
}

int handle_customer(int fd, int id, const char* line) {
    float amount, balance = 0;
    int rc;

    /* rc is stored first on purpose: in reply(fd, op_x(..., &balance), balance)
       C may read `balance` BEFORE op_x runs (argument order is unspecified). */
    if (strcmp(line, "BALANCE") == 0) {
        rc = op_balance(id, &balance);
        reply(fd, rc, balance);
        return 1;
    }
    if (strncmp(line, "DEPOSIT", 7) == 0) {
        if (!parse_amount(line, "DEPOSIT", &amount)) {
            send_line(fd, "ERR Usage: DEPOSIT <amount>\n");
        } else {
            rc = op_deposit(id, amount, &balance);
            reply(fd, rc, balance);
        }
        return 1;
    }
    if (strncmp(line, "WITHDRAW", 8) == 0) {
        if (!parse_amount(line, "WITHDRAW", &amount)) {
            send_line(fd, "ERR Usage: WITHDRAW <amount>\n");
        } else {
            rc = op_withdraw(id, amount, &balance);
            reply(fd, rc, balance);
        }
        return 1;
    }
    if (strncmp(line, "TRANSFER", 8) == 0) {
        int to;
        char extra;
        if (sscanf(line, "TRANSFER %d %f %c", &to, &amount, &extra) != 2) {
            send_line(fd, "ERR Usage: TRANSFER <to_id> <amount>\n");
        } else {
            rc = op_transfer(id, to, amount, &balance);   /* from = session id */
            reply(fd, rc, balance);
        }
        return 1;
    }
    return 0;
}
