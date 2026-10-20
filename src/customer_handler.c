#include <stdio.h>
#include <string.h>
#include "customer_handler.h"
#include "customer_ops.h"
#include "txn_store.h"
#include "net_util.h"
#include "config.h"
#include "loan_ops.h"
#include "loan_store.h"

#define HISTORY_MAX 10   /* show the last 10 transactions */

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

#define LOANS_SHOW_MAX 20

/* Customer loan commands. Returns 1 if the line was a loan command. */
static int handle_loan_cmd(int fd, int id, const char* line) {
    char msg[MAX_LINE_LEN];
    if (strncmp(line, "LOAN_APPLY", 10) == 0) {
        float amount;
        int loan_id = 0;
        if (!parse_amount(line, "LOAN_APPLY", &amount)) {
            send_line(fd, "ERR Usage: LOAN_APPLY <amount>\n");
            return 1;
        }
        int rc = loan_apply(id, amount, &loan_id);     /* id = logged-in user */
        if (rc == LN_OK) {
            snprintf(msg, sizeof(msg), "OK Loan %d applied, status PENDING\n", loan_id);
        } else {
            snprintf(msg, sizeof(msg), "%s\n", loan_message(rc));
        }
        send_line(fd, msg);
        return 1;
    }
    if (strcmp(line, "LOAN_CHECK") == 0) {
        /* UX only: early hint before asking the amount. The real check
           is repeated inside loan_apply() under the loans.bin lock. */
        int n = loans_open_of_customer(id);
        if (n < 0) {
            send_line(fd, "ERR Internal error\n");
        } else if (n > 0) {
            send_line(fd, "ERR You already have an open loan (PENDING or ASSIGNED)\n");
        } else {
            send_line(fd, "OK You can apply\n");
        }
        return 1;
    }
    if (strcmp(line, "MY_LOANS") == 0) {
        struct Loan l[LOANS_SHOW_MAX];
        int n = loan_list_customer(id, l, LOANS_SHOW_MAX);
        if (n < 0) {
            send_line(fd, "ERR Internal error\n");
            return 1;
        }
        snprintf(msg, sizeof(msg), "OK %d\n", n);       /* same format as HISTORY */
        send_line(fd, msg);
        for (int i = 0; i < n; i++) {
            snprintf(msg, sizeof(msg), "Loan %-4d  %10.2f  %-9s  employee %d\n",
                     l[i].loanID, l[i].amount, l[i].status, l[i].assignedEmpID);
            send_line(fd, msg);
        }
        return 1;
    }
    return 0;
}

int handle_customer(int fd, int id, const char* line) {
    float amount, balance = 0;
    int rc;

    if (handle_loan_cmd(fd, id, line)) {
        return 1;
    }

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
    if (strncmp(line, "PASSWD", 6) == 0) {
        char old_pw[64], new_pw[64], extra;
        if (sscanf(line, "PASSWD %63s %63s %c", old_pw, new_pw, &extra) != 2) {
            send_line(fd, "ERR Usage: PASSWD <old> <new>\n");
            return 1;
        }
        rc = op_change_password(id, old_pw, new_pw);
        if (rc == OP_OK) {
            send_line(fd, "OK Password changed\n");
        } else {
            char msg[MAX_LINE_LEN];
            snprintf(msg, sizeof(msg), "%s\n", op_message(rc));
            send_line(fd, msg);
        }
        return 1;
    }
    if (strcmp(line, "HISTORY") == 0) {
        /* Multi-line reply: "OK <n>" first, then exactly n lines.
           The count tells the client how many lines to read next. */
        struct Transaction t[HISTORY_MAX];
        char msg[MAX_LINE_LEN];
        int n = txn_history(id, t, HISTORY_MAX);
        if (n < 0) {
            send_line(fd, "ERR Internal error\n");
            return 1;
        }
        snprintf(msg, sizeof(msg), "OK %d\n", n);
        send_line(fd, msg);
        for (int i = 0; i < n; i++) {
            snprintf(msg, sizeof(msg), "%s  %-12s  %10.2f\n",
                     t[i].timestamp, t[i].type, t[i].amount);
            send_line(fd, msg);
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
