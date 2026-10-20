/* Creates fresh data files.
   ./seed       -> demo data: 1 admin, 2 managers, 3 employees, 50 customers
   ./seed test  -> small data used by the test scripts (fixed IDs)
   Both clear loans, feedback, transactions and the journal. */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "entities.h"
#include "hash.h"

#define FULL_CUSTOMERS 50

/* Writes `count` records of `size` bytes each into a fresh file. */
static int write_records(const char* path, const void* buf, size_t size, int count) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror(path);
        return -1;
    }
    ssize_t total = (ssize_t)(size * count);
    if (total > 0 && write(fd, buf, total) != total) {
        perror("write");
        close(fd);
        return -1;
    }
    fsync(fd);
    close(fd);
    printf("%-22s %d record(s)\n", path, count);
    return 0;
}

static void set(char* dst, const char* src, size_t len) {
    strncpy(dst, src, len - 1);
    dst[len - 1] = '\0';
}

int main(int argc, char** argv) {
    int test_mode = (argc > 1 && strcmp(argv[1], "test") == 0);
    int n_mgr  = test_mode ? 1 : 2;
    int n_emp  = test_mode ? 1 : 3;
    int n_cust = test_mode ? 2 : FULL_CUSTOMERS;

    static struct Admin admins[1];
    static struct Manager managers[2];
    static struct Employee employees[3];
    static struct Customer customers[FULL_CUSTOMERS];
    struct Counter counter;
    memset(&counter, 0, sizeof(counter));      /* static arrays are already zero */

    admins[0].adminID = 1;
    set(admins[0].first_name, "Admin", NAME_LEN);
    set(admins[0].last_name, "One", NAME_LEN);
    hash_password("admin123", admins[0].password);

    const char* nth[3] = {"One", "Two", "Three"};
    const char* mgr_pw[2] = {"manager123", "manager223"};
    const char* emp_pw[3] = {"emp123", "emp223", "emp323"};
    for (int i = 0; i < n_mgr; i++) {
        managers[i].managerID = i + 1;
        set(managers[i].first_name, "Manager", NAME_LEN);
        set(managers[i].last_name, nth[i], NAME_LEN);
        hash_password(mgr_pw[i], managers[i].password);
        set(managers[i].status, ST_ACTIVE, STATUS_LEN);
    }
    for (int i = 0; i < n_emp; i++) {
        employees[i].employeeID = i + 1;
        set(employees[i].first_name, "Employee", NAME_LEN);
        set(employees[i].last_name, nth[i], NAME_LEN);
        hash_password(emp_pw[i], employees[i].password);
        set(employees[i].status, ST_ACTIVE, STATUS_LEN);
    }
    for (int i = 0; i < n_cust; i++) {
        struct Customer* c = &customers[i];
        char last[NAME_LEN], pw[32];
        c->userID = i + 1;
        if (i == 0) {
            set(c->firstName, "Alice", NAME_LEN); set(c->lastName, "Sharma", NAME_LEN);
            hash_password("alice123", c->password); c->balance = 5000.0f;
        } else if (i == 1) {
            set(c->firstName, "Bob", NAME_LEN); set(c->lastName, "Verma", NAME_LEN);
            hash_password("bob123", c->password); c->balance = 10000.0f;
        } else {
            snprintf(last, sizeof(last), "No%d", i + 1);
            snprintf(pw, sizeof(pw), "cust%d", i + 1);
            set(c->firstName, "Customer", NAME_LEN); set(c->lastName, last, NAME_LEN);
            hash_password(pw, c->password); c->balance = 1000.0f * (float)(i % 10 + 1);
        }
        c->loan = 0.0f;
        set(c->status, ST_ACTIVE, STATUS_LEN);
    }

    counter.next_admin_id = 2;
    counter.next_manager_id = n_mgr + 1;
    counter.next_employee_id = n_emp + 1;
    counter.next_customer_id = n_cust + 1;
    counter.next_loan_id = 1;
    counter.next_feedback_id = 1;
    counter.next_transaction_id = 1;

    if (write_records("data/admins.bin", admins, sizeof(struct Admin), 1) < 0 ||
        write_records("data/managers.bin", managers, sizeof(struct Manager), n_mgr) < 0 ||
        write_records("data/employees.bin", employees, sizeof(struct Employee), n_emp) < 0 ||
        write_records("data/customers.bin", customers, sizeof(struct Customer), n_cust) < 0 ||
        write_records("data/counter.bin", &counter, sizeof(struct Counter), 1) < 0 ||
        write_records("data/transactions.bin", NULL, sizeof(struct Transaction), 0) < 0 ||
        write_records("data/loans.bin", NULL, sizeof(struct Loan), 0) < 0 ||
        write_records("data/feedback.bin", NULL, sizeof(struct Feedback), 0) < 0) {
        return 1;
    }
    unlink("data/journal.bin");                /* old journal must never touch new data */
    printf("Seed done (%s data).\n", test_mode ? "test" : "demo");
    return 0;
}
