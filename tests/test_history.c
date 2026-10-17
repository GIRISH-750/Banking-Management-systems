/* Run ./seed first. Checks that every operation is logged, that history
   returns the right customer's entries, and that parallel deposits get
   unique transaction IDs (the counter is locked). */
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "customer_ops.h"
#include "txn_store.h"

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
    int id = *(int*)arg;
    float b;
    for (int i = 0; i < 200; i++) {
        op_deposit(id, 1.0f, &b);
    }
    return NULL;
}

int main(void) {
    static struct Transaction h[500], hb[500];
    float b;

    op_deposit(1, 100, &b);
    op_withdraw(1, 50, &b);
    op_transfer(1, 2, 25, &b);
    op_withdraw(1, 999999, &b);            /* fails: must NOT be logged */

    int n = txn_history(1, h, 10);
    check(n == 3, "Alice has 3 transactions (failed one not logged)");
    for (int i = 0; i < n; i++) {
        printf("      #%d %-12s %8.2f  %s\n", h[i].transactionID, h[i].type, h[i].amount, h[i].timestamp);
    }
    check(n == 3 && strcmp(h[0].type, "DEPOSIT") == 0 && h[0].amount == 100, "1st = DEPOSIT 100");
    check(n == 3 && strcmp(h[1].type, "WITHDRAW") == 0 && h[1].amount == 50, "2nd = WITHDRAW 50");
    check(n == 3 && strcmp(h[2].type, "TRANSFER_OUT") == 0 && h[2].amount == 25, "3rd = TRANSFER_OUT 25");

    n = txn_history(2, h, 10);
    check(n == 1 && strcmp(h[0].type, "TRANSFER_IN") == 0, "Bob has TRANSFER_IN 25");

    n = txn_history(1, h, 2);
    check(n == 2 && strcmp(h[1].type, "TRANSFER_OUT") == 0, "limit 2 returns the LATEST 2");

    /* concurrency: Alice and Bob each deposit 200 times in parallel */
    int a = 1, c = 2;
    pthread_t t1, t2;
    pthread_create(&t1, NULL, depositor, &a);
    pthread_create(&t2, NULL, depositor, &c);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    int na = txn_history(1, h, 500);
    int nb = txn_history(2, hb, 500);
    check(na == 203, "Alice: 203 entries after parallel deposits");
    check(nb == 201, "Bob: 201 entries after parallel deposits");

    /* every transaction ID 1..404 must appear exactly once */
    int seen[405] = {0}, bad = 0;
    for (int i = 0; i < na; i++) { int id = h[i].transactionID;  if (id < 1 || id > 404 || seen[id]++) bad = 1; }
    for (int i = 0; i < nb; i++) { int id = hb[i].transactionID; if (id < 1 || id > 404 || seen[id]++) bad = 1; }
    check(!bad && na + nb == 404, "all 404 transaction IDs unique (counter is safe)");

    printf("%d/%d passed\n", passed, total);
    return passed == total ? 0 : 1;
}
