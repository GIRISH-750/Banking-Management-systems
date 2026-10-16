#ifndef CUSTOMER_OPS_H
#define CUSTOMER_OPS_H

/* Result codes for customer operations */
#define OP_OK             0
#define OP_NOT_FOUND      1
#define OP_INACTIVE       2
#define OP_BAD_AMOUNT     3
#define OP_NO_FUNDS       4
#define OP_ERROR         -1

/* Each function does: open -> lock -> read -> (change -> write) -> unlock -> close.
   On OP_OK, *balance holds the balance after the operation. */
int op_balance(int id, float* balance);
int op_deposit(int id, float amount, float* balance);
int op_withdraw(int id, float amount, float* balance);

/* Turns a result code into a reply line for the client. */
const char* op_message(int rc);

#endif
