/* Day 5 break-it tests: real threads call the ops functions at the same instant.
   Needs fresh data (run ./seed first, server stopped). */
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "loan_ops.h"
#include "customer_ops.h"
#include "admin_ops.h"
#include "rec_store.h"

static int pass = 0, fail = 0;
static pthread_barrier_t bar;
static int res[16];
static int g_loan, g_emp;

static void check(const char* name, int ok) {
    printf("%s  %s\n", ok ? "PASS" : "FAIL", name);
    if (ok) pass++; else fail++;
}

static void* t_apply(void* a) {
    int id;
    pthread_barrier_wait(&bar);                 /* all threads start together */
    res[(long)a] = loan_apply(1, 1000.0f, &id);
    return NULL;
}
static void* t_approve(void* a) {
    pthread_barrier_wait(&bar);
    res[(long)a] = loan_decide(g_loan, 1, 1);
    return NULL;
}
static void* t_assign(void* a) {
    (void)a;
    pthread_barrier_wait(&bar);
    res[0] = loan_assign(g_loan, g_emp);
    return NULL;
}
static void* t_promote(void* a) {
    int mid;
    (void)a;
    pthread_barrier_wait(&bar);
    res[1] = adm_promote(g_emp, &mid);
    return NULL;
}

static void run_same(void* (*f)(void*), int n) {
    pthread_t t[16];
    pthread_barrier_init(&bar, NULL, n);
    for (long i = 0; i < n; i++) pthread_create(&t[i], NULL, f, (void*)i);
    for (int i = 0; i < n; i++) pthread_join(t[i], NULL);
    pthread_barrier_destroy(&bar);
}

static void run_pair(void* (*f)(void*), void* (*g)(void*)) {
    pthread_t a, b;
    pthread_barrier_init(&bar, NULL, 2);
    pthread_create(&a, NULL, f, NULL);
    pthread_create(&b, NULL, g, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    pthread_barrier_destroy(&bar);
}

/* Status of loan `id` of customer `cust` ("" if not found). */
static const char* loan_status(int cust, int id, struct Loan* out) {
    static struct Loan l[100];
    int n = loan_list_customer(cust, l, 100);
    for (int i = 0; i < n; i++) {
        if (l[i].loanID == id) { if (out) *out = l[i]; return l[i].status; }
    }
    return "";
}

static int open_loan_of(int cust) {
    struct Loan l[100];
    int n = loan_list_customer(cust, l, 100);
    for (int i = 0; i < n; i++) {
        if (!strcmp(l[i].status, LOAN_PENDING) || !strcmp(l[i].status, LOAN_ASSIGNED)) return l[i].loanID;
    }
    return 0;
}

int main(void) {
    /* 1. ten parallel applies by the same customer -> exactly one loan */
    run_same(t_apply, 10);
    int ok = 0, has_open = 0;
    for (int i = 0; i < 10; i++) { ok += (res[i] == LN_OK); has_open += (res[i] == LN_HAS_OPEN); }
    check("10 parallel applies -> exactly 1 succeeds", ok == 1 && has_open == 9);
    g_loan = open_loan_of(1);

    /* 2. five parallel approvals of the same loan -> money added once */
    float b0 = 0, b1 = 0;
    loan_assign(g_loan, 1);
    op_balance(1, &b0);
    run_same(t_approve, 5);
    ok = 0;
    int bad = 0;
    for (int i = 0; i < 5; i++) { ok += (res[i] == LN_OK); bad += (res[i] == LN_BAD_STATE); }
    op_balance(1, &b1);
    check("5 parallel approvals -> exactly 1 succeeds", ok == 1 && bad == 4);
    check("balance increased exactly once (+1000)", fabsf(b1 - b0 - 1000.0f) < 0.01f);

    /* 3. wrong employee cannot decide */
    int l2;
    loan_apply(2, 500.0f, &l2);
    loan_assign(l2, 1);
    check("other employee cannot approve (NOT_YOURS)", loan_decide(l2, 99, 1) == LN_NOT_YOURS);
    check("assigned employee can reject", loan_decide(l2, 1, 0) == LN_OK);

    /* 4. assign vs promote at the same instant, 20 rounds:
          never "loan assigned to an employee who was promoted" */
    int good = 0, assign_won = 0, promote_won = 0;
    for (int r = 0; r < 20; r++) {
        int e, l;
        if (adm_add_employee("Race", "Emp", "pass1234", &e) != ADM_OK) { printf("setup failed\n"); break; }
        loan_apply(2, 100.0f, &l);
        g_loan = l;
        g_emp = e;
        run_pair(t_assign, t_promote);
        struct Employee emp;
        int fd = rec_open(EMPLOYEES_FILE);
        rec_read(fd, e, &emp, sizeof(emp));
        close(fd);
        struct Loan ln;
        loan_status(2, l, &ln);
        int a_case = res[0] == LN_OK && res[1] == ADM_HAS_LOANS &&
                     !strcmp(emp.status, ST_ACTIVE) && ln.assignedEmpID == e;
        int p_case = res[1] == ADM_OK && res[0] == LN_EMP_INACTIVE &&
                     !strcmp(emp.status, ST_INACTIVE) && !strcmp(ln.status, LOAN_PENDING);
        good += (a_case || p_case);
        assign_won += a_case;
        promote_won += p_case;
        if (!strcmp(ln.status, LOAN_ASSIGNED)) {          /* clean up: close the loan */
            loan_decide(l, ln.assignedEmpID, 0);
        } else {
            loan_assign(l, 1);
            loan_decide(l, 1, 0);
        }
    }
    printf("      (assign won %d rounds, promote won %d rounds)\n", assign_won, promote_won);
    check("assign vs promote: 20/20 rounds consistent", good == 20);

    printf("--------------------------------\nDay 5 race tests: %d passed, %d failed\n", pass, fail);
    return fail != 0;
}
