/* Run ./seed first (Alice id 1 = 5000, Bob id 2 = 10000).
   Checks the rules, then runs 1->2 and 2->1 transfers at the same time
   (the classic deadlock case). Must finish, and total money must not change. */
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

struct job { int from, to; };

static void* mover(void* arg) {
    struct job* j = arg;
    float b;
    for (int i = 0; i < 1000; i++) {
        op_transfer(j->from, j->to, 1.0f, &b);
    }
    return NULL;
}

int main(void) {
    float a, b, bal;
    op_balance(1, &a);
    op_balance(2, &b);
    float total_before = a + b;
    printf("      -> start: Alice %.2f, Bob %.2f, total %.2f\n", a, b, total_before);

    check(op_transfer(1, 2, 500, &bal) == OP_OK && bal == a - 500, "Alice -> Bob 500");
    op_balance(2, &bal);
    check(bal == b + 500, "Bob received 500");
    check(op_transfer(1, 1, 10, &bal) == OP_SAME_ACCOUNT, "transfer to self rejected");
    check(op_transfer(1, 999, 10, &bal) == OP_NOT_FOUND, "unknown receiver");
    check(op_transfer(1, 2, -5, &bal) == OP_BAD_AMOUNT, "negative amount rejected");
    check(op_transfer(1, 2, 999999, &bal) == OP_NO_FUNDS, "more than balance rejected");
    op_balance(1, &a);
    op_balance(2, &b);
    check(a + b == total_before, "failed transfers changed nothing");

    /* deadlock test: opposite directions at the same time */
    struct job j1 = {1, 2}, j2 = {2, 1};
    pthread_t t1, t2;
    pthread_create(&t1, NULL, mover, &j1);
    pthread_create(&t2, NULL, mover, &j2);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    check(1, "1->2 and 2->1 together finished (no deadlock)");

    op_balance(1, &a);
    op_balance(2, &b);
    printf("      -> end: Alice %.2f, Bob %.2f, total %.2f\n", a, b, a + b);
    check(a + b == total_before, "total money unchanged (no money created or lost)");

    op_transfer(2, 1, 500, &bal);   /* put the 500 back */

    printf("%d/%d passed\n", passed, total);
    return passed == total ? 0 : 1;
}
