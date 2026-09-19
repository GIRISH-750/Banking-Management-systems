#ifndef ENTITIES_H
#define ENTITIES_H

#define NAME_LEN 50
#define PASS_LEN 20
#define MSG_LEN 200

struct Account {
    int acc_no;
    char name[NAME_LEN];
    char password[PASS_LEN];
    float balance;
    int is_joint;
    int joint_holder_id;
    int is_active;
};

struct Employee {
    int emp_id;
    char name[NAME_LEN];
    char password[PASS_LEN];
};

struct Manager {
    int mgr_id;
    char name[NAME_LEN];
    char password[PASS_LEN];
};

struct Admin {
    int admin_id;
    char name[NAME_LEN];
    char password[PASS_LEN];
};

struct Loan {
    int loan_id;
    int acc_no;
    float amount;
    int status;            // 0=pending, 1=approved, 2=rejected
    int assigned_emp_id;   // -1 = unassigned
};

struct Transaction {
    int txn_id;
    int acc_no;
    char type[15];
    float amount;
    long timestamp;
};

struct Feedback {
    int feedback_id;
    int acc_no;
    char message[MSG_LEN];
    int reviewed;
};

struct Counter {
    int next_acc_no;
    int next_loan_id;
    int next_txn_id;
    int next_feedback_id;
};

#endif
