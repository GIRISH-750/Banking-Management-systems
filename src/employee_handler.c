#include <stdio.h>
#include <string.h>
#include "employee_handler.h"
#include "employee_ops.h"
#include "net_util.h"
#include "config.h"

#define PASSBOOK_MAX 50

static void reply_error(int fd, int rc) {
    char msg[MAX_LINE_LEN];
    snprintf(msg, sizeof(msg), "%s\n", emp_message(rc));
    send_line(fd, msg);
}

int handle_employee(int fd, int id, const char* line) {
    (void)id;   /* used on Day 5 for loans */
    char first[64], last[64], pw[64], extra;
    char msg[MAX_LINE_LEN];
    int cid, rc;

    if (strncmp(line, "ADD_CUSTOMER", 12) == 0) {
        if (sscanf(line, "ADD_CUSTOMER %63s %63s %63s %c", first, last, pw, &extra) != 3) {
            send_line(fd, "ERR Usage: ADD_CUSTOMER <first> <last> <password>\n");
            return 1;
        }
        int new_id;
        rc = emp_add_customer(first, last, pw, &new_id);
        if (rc == EMP_OK) {
            snprintf(msg, sizeof(msg), "OK %d\n", new_id);
            send_line(fd, msg);
        } else {
            reply_error(fd, rc);
        }
        return 1;
    }
    if (strncmp(line, "MODIFY_CUSTOMER", 15) == 0) {
        if (sscanf(line, "MODIFY_CUSTOMER %d %63s %63s %c", &cid, first, last, &extra) != 3) {
            send_line(fd, "ERR Usage: MODIFY_CUSTOMER <id> <first> <last>\n");
            return 1;
        }
        rc = emp_modify_customer(cid, first, last);
        if (rc == EMP_OK) {
            send_line(fd, "OK Customer updated\n");
        } else {
            reply_error(fd, rc);
        }
        return 1;
    }
    if (strncmp(line, "PASSBOOK", 8) == 0) {
        if (sscanf(line, "PASSBOOK %d %c", &cid, &extra) != 1) {
            send_line(fd, "ERR Usage: PASSBOOK <customer_id>\n");
            return 1;
        }
        struct Transaction t[PASSBOOK_MAX];
        int n = 0;
        rc = emp_passbook(cid, t, PASSBOOK_MAX, &n);
        if (rc != EMP_OK) {
            reply_error(fd, rc);
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
    return 0;
}
