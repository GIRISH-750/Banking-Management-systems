/* Run ./seed first. Checks rules, then 2 threads x 1000 deposits of 1
   on the same account: with locking, nothing may be lost. */
#include <stdio.h>
#include <pthread.h>
#include "customer_ops.h"

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

static void* depositor(void* arg) {
    (void)arg;
    float b;
    for (int i = 0; i < 1000; i++) {
        op_deposit(1, 1.0f, &b);
    }
    return NULL;
}

int main(void) {
    float b, start;

    check(op_balance(1, &start) == OP_OK, "balance of customer 1");
    printf("      -> start balance %.2f\n", start);

    check(op_deposit(1, 500, &b) == OP_OK && b == start + 500, "deposit 500");
    check(op_withdraw(1, 500, &b) == OP_OK && b == start, "withdraw 500");
    check(op_withdraw(1, 999999, &b) == OP_NO_FUNDS, "withdraw more than balance");
    check(op_deposit(1, -50, &b) == OP_BAD_AMOUNT, "negative deposit rejected");
    check(op_deposit(1, 0, &b) == OP_BAD_AMOUNT, "zero deposit rejected");
    check(op_deposit(999, 10, &b) == OP_NOT_FOUND, "unknown account");

    /* concurrency: lost updates would make the total less than +2000 */
    pthread_t t1, t2;
    pthread_create(&t1, NULL, depositor, NULL);
    pthread_create(&t2, NULL, depositor, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    op_balance(1, &b);
    printf("      -> after 2x1000 parallel deposits: %.2f (expected %.2f)\n", b, start + 2000);
    check(b == start + 2000, "no lost updates under concurrency");

    op_withdraw(1, 2000, &b);   /* put the balance back */

    printf("%d/%d passed\n", passed, total);
    return passed == total ? 0 : 1;
}
