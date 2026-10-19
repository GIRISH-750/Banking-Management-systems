#ifndef CUSTOMER_OPS_H
#define CUSTOMER_OPS_H

/* Result codes for customer operations */
#define OP_OK             0
#define OP_NOT_FOUND      1
#define OP_INACTIVE       2
#define OP_BAD_AMOUNT     3
#define OP_NO_FUNDS       4
#define OP_SAME_ACCOUNT   5
#define OP_WRONG_PASSWORD 6
#define OP_BAD_PASSWORD   7
#define OP_ERROR         -1

/* Each function does: open -> lock -> read -> (change -> write) -> unlock -> close.
   On OP_OK, *balance holds the balance after the operation. */
int op_balance(int id, float* balance);
int op_deposit(int id, float amount, float* balance);
int op_withdraw(int id, float amount, float* balance);

/* Moves amount from `from` to `to`. Both records are locked (lower id first)
   before either is changed. On OP_OK, *balance = sender's new balance. */
int op_transfer(int from, int to, float amount, float* balance);

/* Changes the password after checking the old one. Stores only the hash. */
int op_change_password(int id, const char* old_pw, const char* new_pw);

/* Turns a result code into a reply line for the client. */
const char* op_message(int rc);

#endif
