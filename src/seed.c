#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "entities.h"

/* Writes `count` records of `size` bytes each into a fresh file. */
static int write_records(const char* path, const void* buf, size_t size, int count) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror(path);
        return -1;
    }
    ssize_t total = (ssize_t)(size * count);
    if (write(fd, buf, total) != total) {
        perror("write");
        close(fd);
        return -1;
    }
    close(fd);
    printf("%-22s %d record(s), %zd bytes\n", path, count, total);
    return 0;
}

int main(void) {
    struct Admin admins[1];
    struct Manager managers[1];
    struct Employee employees[1];
    struct Customer customers[2];
    struct Counter counter;

    /* Zero everything first so the files have no garbage bytes. */
    memset(admins, 0, sizeof(admins));
    memset(managers, 0, sizeof(managers));
    memset(employees, 0, sizeof(employees));
    memset(customers, 0, sizeof(customers));
    memset(&counter, 0, sizeof(counter));

    /* Admin: ID 1 */
    admins[0].adminID = 1;
    strncpy(admins[0].first_name, "Admin", NAME_LEN - 1);
    strncpy(admins[0].last_name, "One", NAME_LEN - 1);
    strncpy(admins[0].password, "admin123", PASS_LEN - 1);

    /* Manager: ID 1 */
    managers[0].managerID = 1;
    strncpy(managers[0].first_name, "Manager", NAME_LEN - 1);
    strncpy(managers[0].last_name, "One", NAME_LEN - 1);
    strncpy(managers[0].password, "manager123", PASS_LEN - 1);

    /* Employee: ID 1 */
    employees[0].employeeID = 1;
    strncpy(employees[0].first_name, "Employee", NAME_LEN - 1);
    strncpy(employees[0].last_name, "One", NAME_LEN - 1);
    strncpy(employees[0].password, "emp123", PASS_LEN - 1);
    strncpy(employees[0].status, "ACTIVE", STATUS_LEN - 1);
    employees[0].loan_count = 0;

    /* Customers: ID 1 and 2 */
    customers[0].userID = 1;
    strncpy(customers[0].firstName, "Alice", NAME_LEN - 1);
    strncpy(customers[0].lastName, "Sharma", NAME_LEN - 1);
    strncpy(customers[0].password, "alice123", PASS_LEN - 1);
    customers[0].balance = 5000.0f;
    customers[0].loan = 0.0f;
    strncpy(customers[0].status, "ACTIVE", STATUS_LEN - 1);

    customers[1].userID = 2;
    strncpy(customers[1].firstName, "Bob", NAME_LEN - 1);
    strncpy(customers[1].lastName, "Verma", NAME_LEN - 1);
    strncpy(customers[1].password, "bob123", PASS_LEN - 1);
    customers[1].balance = 10000.0f;
    customers[1].loan = 0.0f;
    strncpy(customers[1].status, "ACTIVE", STATUS_LEN - 1);

    /* Counter: next free ID for each entity */
    counter.next_admin_id = 2;
    counter.next_manager_id = 2;
    counter.next_employee_id = 2;
    counter.next_customer_id = 3;
    counter.next_loan_id = 1;
    counter.next_feedback_id = 1;
    counter.next_transaction_id = 1;

    if (write_records("data/admins.bin", admins, sizeof(struct Admin), 1) < 0) {
        return 1;
    }
    if (write_records("data/managers.bin", managers, sizeof(struct Manager), 1) < 0) {
        return 1;
    }
    if (write_records("data/employees.bin", employees, sizeof(struct Employee), 1) < 0) {
        return 1;
    }
    if (write_records("data/customers.bin", customers, sizeof(struct Customer), 2) < 0) {
        return 1;
    }
    if (write_records("data/counter.bin", &counter, sizeof(struct Counter), 1) < 0) {
        return 1;
    }

    printf("Seed done.\n");
    return 0;
}
