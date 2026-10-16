/* Checks read, not-found and a locked read-modify-write round trip. */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "customer_store.h"
#include "lock.h"

static int passed = 0, total = 0;

static void check(int cond, const char* name) {
    total++;
    if (cond) {
        passed++;
        printf("PASS  %s\n", name);
    } else {
        printf("FAIL  %s\n", name);
    }
}

int main(void) {
    struct Customer c;
    int fd = customer_open();
    check(fd >= 0, "open customers.bin");

    /* 1. read an existing customer */
    lock_record(fd, 1, sizeof(c), F_RDLCK);
    int rc = customer_read(fd, 1, &c);
    unlock_record(fd, 1, sizeof(c));
    check(rc == STORE_OK && c.userID == 1, "read customer 1");
    printf("      -> %s %s, balance %.2f, %s\n", c.firstName, c.lastName, c.balance, c.status);

    /* 2. ids that do not exist */
    check(customer_read(fd, 0, &c) == STORE_NOT_FOUND, "id 0 not found");
    check(customer_read(fd, 999, &c) == STORE_NOT_FOUND, "id 999 not found");

    /* 3. locked read-modify-write: +100 then -100, balance must come back */
    lock_record(fd, 1, sizeof(c), F_WRLCK);
    customer_read(fd, 1, &c);
    float before = c.balance;
    c.balance += 100.0f;
    customer_write(fd, 1, &c);
    customer_read(fd, 1, &c);
    check(c.balance == before + 100.0f, "write +100 visible");
    c.balance -= 100.0f;
    customer_write(fd, 1, &c);
    unlock_record(fd, 1, sizeof(c));
    customer_read(fd, 1, &c);
    check(c.balance == before, "balance restored");

    close(fd);
    printf("%d/%d passed\n", passed, total);
    return passed == total ? 0 : 1;
}
