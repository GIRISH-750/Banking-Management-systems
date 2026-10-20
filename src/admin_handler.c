#include <stdio.h>
#include <string.h>
#include "admin_handler.h"
#include "admin_ops.h"
#include "employee_ops.h"
#include "net_util.h"
#include "config.h"

/* line starts with cmd, followed by a space or end of line */
static int is_cmd(const char* line, const char* cmd) {
    size_t n = strlen(cmd);
    return strncmp(line, cmd, n) == 0 && (line[n] == ' ' || line[n] == '\0');
}

static void send_rc(int fd, int rc, const char* ok_msg) {
    char msg[MAX_LINE_LEN];
    snprintf(msg, sizeof(msg), "%s\n", rc == ADM_OK ? ok_msg : adm_message(rc));
    send_line(fd, msg);
}

/* For commands that take exactly one ID: returns 1 and fills *id, else sends usage. */
static int one_id(int fd, const char* line, const char* cmd, int* id) {
    char fmt[48], msg[MAX_LINE_LEN], extra;
    snprintf(fmt, sizeof(fmt), "%s %%d %%c", cmd);
    if (sscanf(line, fmt, id, &extra) != 1) {
        snprintf(msg, sizeof(msg), "ERR Usage: %s <id>\n", cmd);
        send_line(fd, msg);
        return 0;
    }
    return 1;
}

#include <fcntl.h>
#include <unistd.h>
#include "config.h"
#include "net_util.h"
#include "rec_store.h"
#include "lock.h"
#include "entities.h"

#define STAFF_MAX 200

/* Reads up to max records under a whole-file READ lock. Returns count or -1. */
static int read_all(const char* path, void* out, size_t size, int max) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    if (lock_file(fd, F_RDLCK) < 0) {
        close(fd);
        return -1;
    }
    int n = 0;
    while (n < max && pread(fd, (char*)out + (size_t)n * size, size, (off_t)n * (off_t)size) == (ssize_t)size) {
        n++;
    }
    unlock_file(fd);
    close(fd);
    return n;
}

/* LIST_STAFF: every employee and manager with status (empty slots skipped). */
static void list_staff(int fd) {
    static struct Employee e[STAFF_MAX];
    static struct Manager m[STAFF_MAX];
    char msg[MAX_LINE_LEN];
    int ne = read_all(EMPLOYEES_FILE, e, sizeof(e[0]), STAFF_MAX);
    int nm = read_all(MANAGERS_FILE, m, sizeof(m[0]), STAFF_MAX);
    if (ne < 0 || nm < 0) {
        send_line(fd, "ERR Internal error\n");
        return;
    }
    int total = 0;
    for (int i = 0; i < ne; i++) total += (e[i].employeeID > 0);
    for (int i = 0; i < nm; i++) total += (m[i].managerID > 0);
    snprintf(msg, sizeof(msg), "OK %d\n", total);
    send_line(fd, msg);
    for (int i = 0; i < nm; i++) {
        if (m[i].managerID > 0) {
            snprintf(msg, sizeof(msg), "Manager  %-3d %-10s %-10s %s\n",
                     m[i].managerID, m[i].first_name, m[i].last_name, m[i].status);
            send_line(fd, msg);
        }
    }
    for (int i = 0; i < ne; i++) {
        if (e[i].employeeID > 0) {
            snprintf(msg, sizeof(msg), "Employee %-3d %-10s %-10s %s\n",
                     e[i].employeeID, e[i].first_name, e[i].last_name, e[i].status);
            send_line(fd, msg);
        }
    }
}

int handle_admin(int fd, int id, const char* line) {
    if (strcmp(line, "LIST_STAFF") == 0) {
        list_staff(fd);
        return 1;
    }
    (void)id;
    char first[64], last[64], pw[64], msg[MAX_LINE_LEN], extra;
    int tid, new_id, rc;

    if (is_cmd(line, "ADD_EMPLOYEE")) {
        if (sscanf(line, "ADD_EMPLOYEE %63s %63s %63s %c", first, last, pw, &extra) != 3) {
            send_line(fd, "ERR Usage: ADD_EMPLOYEE <first> <last> <password>\n");
            return 1;
        }
        rc = adm_add_employee(first, last, pw, &new_id);
        snprintf(msg, sizeof(msg), "OK %d", new_id);
        send_rc(fd, rc, msg);
        return 1;
    }
    if (is_cmd(line, "MODIFY_EMPLOYEE")) {
        if (sscanf(line, "MODIFY_EMPLOYEE %d %63s %63s %c", &tid, first, last, &extra) != 3) {
            send_line(fd, "ERR Usage: MODIFY_EMPLOYEE <id> <first> <last>\n");
            return 1;
        }
        send_rc(fd, adm_modify_employee(tid, first, last), "OK Employee updated");
        return 1;
    }
    if (is_cmd(line, "PROMOTE")) {
        if (one_id(fd, line, "PROMOTE", &tid)) {
            rc = adm_promote(tid, &new_id);
            snprintf(msg, sizeof(msg), "OK %d", new_id);
            send_rc(fd, rc, msg);
        }
        return 1;
    }
    if (is_cmd(line, "DEMOTE")) {
        if (one_id(fd, line, "DEMOTE", &tid)) {
            rc = adm_demote(tid, &new_id);
            snprintf(msg, sizeof(msg), "OK %d", new_id);
            send_rc(fd, rc, msg);
        }
        return 1;
    }
    if (is_cmd(line, "DELETE_EMPLOYEE")) {
        if (one_id(fd, line, "DELETE_EMPLOYEE", &tid)) {
            send_rc(fd, adm_delete_employee(tid), "OK Employee deleted");
        }
        return 1;
    }
    if (is_cmd(line, "DELETE_CUSTOMER")) {
        if (one_id(fd, line, "DELETE_CUSTOMER", &tid)) {
            send_rc(fd, adm_delete_customer(tid), "OK Customer deleted");
        }
        return 1;
    }
    if (is_cmd(line, "VIEW_CUSTOMER")) {
        struct Customer c;
        if (one_id(fd, line, "VIEW_CUSTOMER", &tid)) {
            rc = adm_view_customer(tid, &c);
            snprintf(msg, sizeof(msg), "OK %d %s %s %.2f %.2f %s",
                     c.userID, c.firstName, c.lastName, c.balance, c.loan, c.status);
            send_rc(fd, rc, msg);
        }
        return 1;
    }
    if (is_cmd(line, "MODIFY_CUSTOMER")) {   /* same operation employees use */
        if (sscanf(line, "MODIFY_CUSTOMER %d %63s %63s %c", &tid, first, last, &extra) != 3) {
            send_line(fd, "ERR Usage: MODIFY_CUSTOMER <id> <first> <last>\n");
            return 1;
        }
        rc = emp_modify_customer(tid, first, last);
        snprintf(msg, sizeof(msg), "%s\n", rc == EMP_OK ? "OK Customer updated" : emp_message(rc));
        send_line(fd, msg);
        return 1;
    }
    return 0;
}
