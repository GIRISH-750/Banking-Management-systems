#ifndef ENTITIES_H
#define ENTITIES_H

#define NAME_LEN 50
#define PASS_LEN 65
#define MSG_LEN 200
#define STATUS_LEN 15
#define TYPE_LEN 15
#define TIMESTAMP_LEN 20

/* Account status (Employee, Manager, Customer) */
#define ST_ACTIVE   "ACTIVE"
#define ST_INACTIVE "INACTIVE"

/* Loan status: PENDING -> ASSIGNED -> APPROVED / REJECTED */
#define LOAN_PENDING  "PENDING"
#define LOAN_ASSIGNED "ASSIGNED"
#define LOAN_APPROVED "APPROVED"
#define LOAN_REJECTED "REJECTED"

struct Admin {
    int adminID;
    char first_name[NAME_LEN];
    char last_name[NAME_LEN];
    char password[PASS_LEN];
};

struct Manager {
    int managerID;
    char first_name[NAME_LEN];
    char last_name[NAME_LEN];
    char password[PASS_LEN];
    char status[STATUS_LEN];       /* INACTIVE after demotion */
};

/* No assigned_loans[] here: an employee's loans are found by scanning
   loans.bin for assignedEmpID == employeeID (one source of truth). */
struct Employee {
    int employeeID;
    char first_name[NAME_LEN];
    char last_name[NAME_LEN];
    char password[PASS_LEN];
    char status[STATUS_LEN];       /* INACTIVE after promotion */
};

struct Customer {
    int userID;
    char firstName[NAME_LEN];
    char lastName[NAME_LEN];
    char password[PASS_LEN];
    float balance;
    float loan;
    char status[STATUS_LEN];
};

struct Loan {
    int loanID;
    int userID;                    /* customer who applied */
    float amount;
    char status[STATUS_LEN];
    int assignedEmpID;             /* 0 = not assigned yet */
};

struct Feedback {
    int feedbackID;
    int customerID;
    char message[MSG_LEN];
    char status[STATUS_LEN];
};

struct Transaction {
    int transactionID;
    int customerID;
    char type[TYPE_LEN];
    float amount;
    char timestamp[TIMESTAMP_LEN];
};

struct Counter {
    int next_customer_id;
    int next_employee_id;
    int next_manager_id;
    int next_admin_id;
    int next_loan_id;
    int next_feedback_id;
    int next_transaction_id;
};

#endif
