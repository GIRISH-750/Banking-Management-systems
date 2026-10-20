#ifndef LOAN_OPS_H
#define LOAN_OPS_H
#include "entities.h"

#define LOAN_MAX 1000000.0f

#define LN_OK            0
#define LN_NOT_FOUND     1   /* customer / loan / employee does not exist */
#define LN_INACTIVE      2   /* customer not ACTIVE */
#define LN_BAD_AMOUNT    3
#define LN_HAS_OPEN      4   /* customer already has a PENDING/ASSIGNED loan */
#define LN_BAD_STATE     5   /* loan not in the right status for this action */
#define LN_NOT_YOURS     6   /* loan not assigned to this employee */
#define LN_EMP_INACTIVE  7   /* employee not ACTIVE (e.g. just promoted) */
#define LN_EMP_NOT_FOUND 8   /* employee does not exist / deleted */
#define LN_ERROR        -1

/* Customer: new PENDING loan. One open loan per customer. */
int loan_apply(int cust_id, float amount, int* loan_id);
/* Lists: fill out[] (max entries), return count or -1. */
int loan_list_customer(int cust_id, struct Loan* out, int max);
int loan_list_pending(struct Loan* out, int max);
int loan_list_assigned(int emp_id, struct Loan* out, int max);
/* Manager: PENDING -> ASSIGNED to an ACTIVE employee. */
int loan_assign(int loan_id, int emp_id);
/* Employee: ASSIGNED -> APPROVED (approve=1) or REJECTED (approve=0). */
int loan_decide(int loan_id, int emp_id, int approve);
const char* loan_message(int rc);
#endif
