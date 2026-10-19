#ifndef LOAN_STORE_H
#define LOAN_STORE_H

#define LOANS_FILE "data/loans.bin"

/* Number of loans still ASSIGNED to this employee. -1 on error. */
int loans_assigned_to(int emp_id);

/* Number of this customer's open loans (PENDING or ASSIGNED). -1 on error. */
int loans_open_of_customer(int cust_id);
#endif
