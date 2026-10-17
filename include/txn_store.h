#ifndef TXN_STORE_H
#define TXN_STORE_H

#include "entities.h"

#define TXN_FILE "data/transactions.bin"

/* Transaction types written to the log */
#define TXN_DEPOSIT       "DEPOSIT"
#define TXN_WITHDRAW      "WITHDRAW"
#define TXN_TRANSFER_OUT  "TRANSFER_OUT"
#define TXN_TRANSFER_IN   "TRANSFER_IN"

/* Appends one transaction (new ID from counter.bin, current time).
   Returns 0 on success, -1 on error. */
int txn_log(int customer_id, const char* type, float amount);

/* Fills out[] with the LAST `max` transactions of this customer, oldest
   first. Returns how many were found, or -1 on error. */
int txn_history(int customer_id, struct Transaction* out, int max);

#endif
