#include <stdio.h>
#include <string.h>
#include "manager_handler.h"
#include "loan_ops.h"
#include "net_util.h"
#include "config.h"

#define PENDING_SHOW_MAX 50

int handle_manager(int fd, int id, const char* line) {
    char msg[MAX_LINE_LEN];
    (void)id;                                   /* any active manager may assign */
    if (strcmp(line, "PENDING_LOANS") == 0) {
        struct Loan l[PENDING_SHOW_MAX];
        int n = loan_list_pending(l, PENDING_SHOW_MAX);
        if (n < 0) {
            send_line(fd, "ERR Internal error\n");
            return 1;
        }
        snprintf(msg, sizeof(msg), "OK %d\n", n);
        send_line(fd, msg);
        for (int i = 0; i < n; i++) {
            snprintf(msg, sizeof(msg), "Loan %-4d  customer %-4d  %10.2f\n",
                     l[i].loanID, l[i].userID, l[i].amount);
            send_line(fd, msg);
        }
        return 1;
    }
    if (strcmp(line, "ALL_LOANS") == 0) {
        struct Loan l[PENDING_SHOW_MAX];
        int n = loan_list_all(l, PENDING_SHOW_MAX);
        if (n < 0) {
            send_line(fd, "ERR Internal error\n");
            return 1;
        }
        snprintf(msg, sizeof(msg), "OK %d\n", n);
        send_line(fd, msg);
        for (int i = 0; i < n; i++) {
            snprintf(msg, sizeof(msg), "Loan %-4d  customer %-4d  %10.2f  %-9s  employee %d\n",
                     l[i].loanID, l[i].userID, l[i].amount, l[i].status, l[i].assignedEmpID);
            send_line(fd, msg);
        }
        return 1;
    }
    if (strncmp(line, "ASSIGN", 6) == 0) {
        int loan_id, emp_id;
        char extra;
        if (sscanf(line, "ASSIGN %d %d %c", &loan_id, &emp_id, &extra) != 2) {
            send_line(fd, "ERR Usage: ASSIGN <loan_id> <employee_id>\n");
            return 1;
        }
        int rc = loan_assign(loan_id, emp_id);
        if (rc == LN_OK) {
            snprintf(msg, sizeof(msg), "OK Loan %d assigned to employee %d\n", loan_id, emp_id);
        } else {
            snprintf(msg, sizeof(msg), "%s\n", loan_message(rc));
        }
        send_line(fd, msg);
        return 1;
    }
    return 0;
}
