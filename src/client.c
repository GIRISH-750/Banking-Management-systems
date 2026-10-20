#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include "config.h"
#include "net_util.h"

#define DEFAULT_HOST "127.0.0.1"

static int connect_to_server(const char* host) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid server address: %s\n", host);
        close(fd);
        return -1;
    }
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }
    return fd;
}

/* Reads one line from the keyboard. Returns 0 = ok, -1 = input ended. */
static int ask(const char* prompt, char* buf, size_t size) {
    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buf, (int)size, stdin) == NULL) {
        return -1;
    }
    char* nl = strchr(buf, '\n');
    if (nl != NULL) {
        *nl = '\0';
    } else {
        int c;   /* line was too long: throw away the rest of it */
        while ((c = getchar()) != '\n' && c != EOF) {
        }
    }
    return 0;
}

/* Sends one request line, reads one reply line. -1 if the connection broke. */
static int request(int fd, const char* req, char* reply, size_t size) {
    if (send_line(fd, req) < 0) {
        return -1;
    }
    if (read_line(fd, reply, size) <= 0) {
        return -1;
    }
    return 0;
}

static int has_space(const char* s) {
    for (; *s != '\0'; s++) {
        if (isspace((unsigned char)*s)) {
            return 1;
        }
    }
    return 0;
}

static const char* role_name(int role) {
    switch (role) {
        case 1:  return "Admin";
        case 2:  return "Manager";
        case 3:  return "Employee";
        case 4:  return "Customer";
        default: return "User";
    }
}

/* Returns 1 = logged in, 0 = login failed, -1 = stop (connection lost / input ended). */
static int do_login(int fd, int* role_out, int* id_out) {
    char role_s[32], id_s[32], pw[128], req[MAX_LINE_LEN], reply[MAX_LINE_LEN];
    int role, id;

    printf("Roles: 1 Admin, 2 Manager, 3 Employee, 4 Customer\n");
    if (ask("Role: ", role_s, sizeof(role_s)) < 0) {
        return -1;
    }
    if (ask("ID: ", id_s, sizeof(id_s)) < 0) {
        return -1;
    }
    if (ask("Password: ", pw, sizeof(pw)) < 0) {
        return -1;
    }

    if (sscanf(role_s, "%d", &role) != 1 || role < 1 || role > 4) {
        printf("Invalid role.\n");
        return 0;
    }
    if (sscanf(id_s, "%d", &id) != 1 || id < 1) {
        printf("Invalid ID.\n");
        return 0;
    }
    if (pw[0] == '\0' || strlen(pw) > 63 || has_space(pw)) {
        printf("Password must be 1-63 characters, no spaces.\n");
        return 0;
    }

    snprintf(req, sizeof(req), "LOGIN %d %d %s\n", role, id, pw);
    if (request(fd, req, reply, sizeof(reply)) < 0) {
        return -1;
    }
    printf("Server: %s\n", reply);

    if (strncmp(reply, "OK", 2) == 0) {
        *role_out = role;
        *id_out = id;
        return 1;
    }
    return 0;
}

/* Sends LOGOUT. Returns 0 = logged out, -1 = connection lost. */
static int do_logout(int fd) {
    char reply[MAX_LINE_LEN];
    if (request(fd, "LOGOUT\n", reply, sizeof(reply)) < 0) {
        return -1;
    }
    printf("Server: %s\n", reply);
    return 0;
}

/* Asks for an amount and sends "<cmd> <amount>". Returns -1 if the connection broke. */
static int amount_request(int fd, const char* cmd) {
    char buf[64], req[MAX_LINE_LEN], reply[MAX_LINE_LEN], extra;
    float amount;
    if (ask("Amount: ", buf, sizeof(buf)) < 0) {
        return -1;
    }
    if (sscanf(buf, "%f %c", &amount, &extra) != 1 || !(amount > 0)) {
        printf("Enter a positive number.\n");
        return 0;
    }
    snprintf(req, sizeof(req), "%s %.2f\n", cmd, amount);
    if (request(fd, req, reply, sizeof(reply)) < 0) {
        return -1;
    }
    printf("Server: %s\n", reply);
    return 0;
}

/* Asks for receiver id + amount and sends TRANSFER. Returns -1 if the connection broke. */
static int transfer_request(int fd) {
    char id_s[32], amt_s[64], req[MAX_LINE_LEN], reply[MAX_LINE_LEN], extra;
    int to;
    float amount;
    if (ask("To account ID: ", id_s, sizeof(id_s)) < 0) {
        return -1;
    }
    if (sscanf(id_s, "%d %c", &to, &extra) != 1 || to < 1) {
        printf("Invalid account ID.\n");   /* bad format: stop before asking the amount */
        return 0;
    }
    if (ask("Amount: ", amt_s, sizeof(amt_s)) < 0) {
        return -1;
    }
    if (sscanf(amt_s, "%f %c", &amount, &extra) != 1 || !(amount > 0)) {
        printf("Enter a positive number.\n");
        return 0;
    }
    snprintf(req, sizeof(req), "TRANSFER %d %.2f\n", to, amount);
    if (request(fd, req, reply, sizeof(reply)) < 0) {
        return -1;
    }
    printf("Server: %s\n", reply);
    return 0;
}

/* Sends HISTORY, then reads "OK <n>" and n more lines. -1 if the connection broke. */
static int history_request(int fd) {
    char reply[MAX_LINE_LEN];
    int n;
    if (request(fd, "HISTORY\n", reply, sizeof(reply)) < 0) {
        return -1;
    }
    if (sscanf(reply, "OK %d", &n) != 1) {
        printf("Server: %s\n", reply);
        return 0;
    }
    if (n == 0) {
        printf("No transactions yet.\n");
        return 0;
    }
    printf("Last %d transaction(s):\n", n);
    for (int i = 0; i < n; i++) {
        if (read_line(fd, reply, sizeof(reply)) <= 0) {
            return -1;
        }
        printf("  %s\n", reply);
    }
    return 0;
}

/* Asks old + new password (new one twice) and sends PASSWD. -1 if the connection broke. */
static int password_request(int fd) {
    char old_pw[128], new_pw[128], again[128], req[MAX_LINE_LEN], reply[MAX_LINE_LEN];
    if (ask("Old password: ", old_pw, sizeof(old_pw)) < 0) {
        return -1;
    }
    if (ask("New password: ", new_pw, sizeof(new_pw)) < 0) {
        return -1;
    }
    if (ask("Repeat new password: ", again, sizeof(again)) < 0) {
        return -1;
    }
    if (strcmp(new_pw, again) != 0) {
        printf("Passwords do not match.\n");
        return 0;
    }
    if (old_pw[0] == '\0' || strlen(old_pw) > 63 || strlen(new_pw) < 4 || strlen(new_pw) > 63 || has_space(new_pw) || has_space(old_pw)) {
        printf("Password must be 4-63 characters, no spaces.\n");
        return 0;
    }
    snprintf(req, sizeof(req), "PASSWD %.63s %.63s\n", old_pw, new_pw);   /* %.63s = at most 63 chars */
    if (request(fd, req, reply, sizeof(reply)) < 0) {
        return -1;
    }
    printf("Server: %s\n", reply);
    return 0;
}

/* Sends cmd, then reads "OK <n>" and n more lines (generic list reply).
   Used for MY_LOANS, pending loans, assigned loans. -1 if connection broke. */
static int list_request(int fd, const char* cmd, const char* title, const char* empty_msg) {
    char req[MAX_LINE_LEN], reply[MAX_LINE_LEN];
    int n;
    snprintf(req, sizeof(req), "%s\n", cmd);
    if (request(fd, req, reply, sizeof(reply)) < 0) {
        return -1;
    }
    if (sscanf(reply, "OK %d", &n) != 1) {
        printf("Server: %s\n", reply);
        return 0;
    }
    if (n == 0) {
        printf("%s\n", empty_msg);
        return 0;
    }
    printf("%s (%d):\n", title, n);
    for (int i = 0; i < n; i++) {
        if (read_line(fd, reply, sizeof(reply)) <= 0) {
            return -1;
        }
        printf("  %s\n", reply);
    }
    return 0;
}

/* Asks the server first, so a customer with an open loan is told at once
   instead of after typing an amount. */
static int loan_apply_request(int fd) {
    char reply[MAX_LINE_LEN];
    if (request(fd, "LOAN_CHECK\n", reply, sizeof(reply)) < 0) {
        return -1;
    }
    if (strncmp(reply, "OK", 2) != 0) {
        printf("Server: %s\n", reply);
        return 0;
    }
    return amount_request(fd, "LOAN_APPLY");
}

/* Customer menu. Returns 0 = logged out (back to main menu), -1 = stop. */
/* Asks a feedback message and sends "FEEDBACK <message>". */
static int feedback_request(int fd) {
    char text[256], req[MAX_LINE_LEN], reply[MAX_LINE_LEN];
    if (ask("Your feedback (max 199 chars): ", text, sizeof(text)) < 0) return -1;
    if (text[0] == '\0' || strlen(text) > 199) {
        printf("Feedback must be 1-199 characters.\n");
        return 0;
    }
    snprintf(req, sizeof(req), "FEEDBACK %.199s\n", text);
    if (request(fd, req, reply, sizeof(reply)) < 0) return -1;
    printf("Server: %s\n", reply);
    return 0;
}

static int customer_menu(int fd, int id) {
    char choice[16], reply[MAX_LINE_LEN];
    while (1) {
        printf("\n--- Customer %d ---\n", id);
        printf("1. View balance\n2. Deposit\n3. Withdraw\n4. Transfer\n5. Transaction history\n6. Change password\n7. Apply for loan\n8. My loans\n9. Give feedback\n0. Logout\n");
        if (ask("Choice: ", choice, sizeof(choice)) < 0) {
            return -1;
        }
        int r = 0;
        if (strcmp(choice, "1") == 0) {
            r = request(fd, "BALANCE\n", reply, sizeof(reply));
            if (r == 0) {
                printf("Server: %s\n", reply);
            }
        } else if (strcmp(choice, "2") == 0) {
            r = amount_request(fd, "DEPOSIT");
        } else if (strcmp(choice, "3") == 0) {
            r = amount_request(fd, "WITHDRAW");
        } else if (strcmp(choice, "4") == 0) {
            r = transfer_request(fd);
        } else if (strcmp(choice, "5") == 0) {
            r = history_request(fd);
        } else if (strcmp(choice, "6") == 0) {
            r = password_request(fd);
        } else if (strcmp(choice, "7") == 0) {
            r = loan_apply_request(fd);
        } else if (strcmp(choice, "8") == 0) {
            r = list_request(fd, "MY_LOANS", "Your loans", "No loans yet.");
        } else if (strcmp(choice, "9") == 0) {
            r = feedback_request(fd);
        } else if (strcmp(choice, "0") == 0) {
            return do_logout(fd);
        } else {
            printf("Invalid choice.\n");
        }
        if (r < 0) {
            return -1;
        }
    }
}

/* Menu for roles whose features are not built yet. */

/* ---------- Employee menu ---------- */

/* 1 = ok, 0 = invalid (message printed), -1 = input ended */
static int ask_id(const char* prompt, int* out) {
    char buf[32], *end;
    if (ask(prompt, buf, sizeof(buf)) < 0) {
        return -1;
    }
    long v = strtol(buf, &end, 10);
    if (buf[0] == '\0' || *end != '\0' || v <= 0 || v > 1000000) {
        printf("Invalid ID.\n");
        return 0;
    }
    *out = (int)v;
    return 1;
}

/* 1 = ok: not empty, at most max chars, no spaces */
static int valid_word(const char* s, size_t max) {
    return s[0] != '\0' && strlen(s) <= max && !has_space(s);
}

static int add_customer_request(int fd) {
    char first[128], last[128], pw[128], req[MAX_LINE_LEN], reply[MAX_LINE_LEN];
    if (ask("First name: ", first, sizeof(first)) < 0) return -1;
    if (ask("Last name: ", last, sizeof(last)) < 0) return -1;
    if (ask("Initial password: ", pw, sizeof(pw)) < 0) return -1;
    if (!valid_word(first, 49) || !valid_word(last, 49)) {
        printf("Names must be 1-49 characters, no spaces.\n");
        return 0;
    }
    if (strlen(pw) < 4 || !valid_word(pw, 63)) {
        printf("Password must be 4-63 characters, no spaces.\n");
        return 0;
    }
    snprintf(req, sizeof(req), "ADD_CUSTOMER %.49s %.49s %.63s\n", first, last, pw);
    if (request(fd, req, reply, sizeof(reply)) < 0) return -1;
    int new_id;
    if (sscanf(reply, "OK %d", &new_id) == 1) {
        printf("Customer created. New customer ID: %d\n", new_id);
    } else {
        printf("Server: %s\n", reply);
    }
    return 0;
}

static int modify_customer_request(int fd) {
    char first[128], last[128], req[MAX_LINE_LEN], reply[MAX_LINE_LEN];
    int cid, r = ask_id("Customer ID: ", &cid);
    if (r <= 0) return r;
    if (ask("New first name: ", first, sizeof(first)) < 0) return -1;
    if (ask("New last name: ", last, sizeof(last)) < 0) return -1;
    if (!valid_word(first, 49) || !valid_word(last, 49)) {
        printf("Names must be 1-49 characters, no spaces.\n");
        return 0;
    }
    snprintf(req, sizeof(req), "MODIFY_CUSTOMER %d %.49s %.49s\n", cid, first, last);
    if (request(fd, req, reply, sizeof(reply)) < 0) return -1;
    printf("Server: %s\n", reply);
    return 0;
}

static int passbook_request(int fd) {
    char req[MAX_LINE_LEN], reply[MAX_LINE_LEN];
    int cid, n, r = ask_id("Customer ID: ", &cid);
    if (r <= 0) return r;
    snprintf(req, sizeof(req), "PASSBOOK %d\n", cid);
    if (request(fd, req, reply, sizeof(reply)) < 0) return -1;
    if (sscanf(reply, "OK %d", &n) != 1) {
        printf("Server: %s\n", reply);
        return 0;
    }
    if (n == 0) {
        printf("No transactions yet.\n");
        return 0;
    }
    printf("Passbook of customer %d (%d transaction(s)):\n", cid, n);
    for (int i = 0; i < n; i++) {
        if (read_line(fd, reply, sizeof(reply)) <= 0) return -1;
        printf("  %s\n", reply);
    }
    return 0;
}

/* Returns 0 = logged out (back to main menu), -1 = stop. */
/* Asks a loan ID, sends "<cmd> <id>" (APPROVE / REJECT). -1 if connection broke. */
static int decide_request(int fd, const char* cmd) {
    char req[MAX_LINE_LEN], reply[MAX_LINE_LEN];
    int loan_id, r;
    if ((r = ask_id("Loan ID: ", &loan_id)) <= 0) return r;
    snprintf(req, sizeof(req), "%s %d\n", cmd, loan_id);
    if (request(fd, req, reply, sizeof(reply)) < 0) return -1;
    printf("Server: %s\n", reply);
    return 0;
}

static int employee_menu(int fd, int id) {
    char choice[16];
    while (1) {
        printf("\n--- Employee %d ---\n", id);
        printf("1. Add customer\n2. Modify customer\n3. View customer passbook\n4. Change password\n5. View assigned loans\n6. Approve loan\n7. Reject loan\n0. Logout\n");
        if (ask("Choice: ", choice, sizeof(choice)) < 0) return -1;
        int r;
        if (strcmp(choice, "1") == 0) {
            r = add_customer_request(fd);
        } else if (strcmp(choice, "2") == 0) {
            r = modify_customer_request(fd);
        } else if (strcmp(choice, "3") == 0) {
            r = passbook_request(fd);
        } else if (strcmp(choice, "4") == 0) {
            r = password_request(fd);
        } else if (strcmp(choice, "5") == 0) {
            r = list_request(fd, "MY_ASSIGNED", "Loans assigned to you", "No loans assigned to you.");
        } else if (strcmp(choice, "6") == 0) {
            r = decide_request(fd, "APPROVE");
        } else if (strcmp(choice, "7") == 0) {
            r = decide_request(fd, "REJECT");
        } else if (strcmp(choice, "0") == 0) {
            return do_logout(fd);
        } else {
            printf("Invalid choice.\n");
            continue;
        }
        if (r < 0) return -1;
    }
}
/* ---------- Admin + Manager menus ---------- */

/* Asks an ID, sends "<cmd> <id>", prints the reply.
   If ok_label is given and reply is "OK <n>", prints "<ok_label> <n>". */
static int id_command(int fd, const char* cmd, const char* prompt, const char* ok_label) {
    char req[MAX_LINE_LEN], reply[MAX_LINE_LEN];
    int tid, n, r = ask_id(prompt, &tid);
    if (r <= 0) return r;
    snprintf(req, sizeof(req), "%s %d\n", cmd, tid);
    if (request(fd, req, reply, sizeof(reply)) < 0) return -1;
    if (ok_label != NULL && sscanf(reply, "OK %d", &n) == 1) {
        printf("%s %d\n", ok_label, n);
    } else {
        printf("Server: %s\n", reply);
    }
    return 0;
}

static int add_employee_request(int fd) {
    char first[128], last[128], pw[128], req[MAX_LINE_LEN], reply[MAX_LINE_LEN];
    if (ask("First name: ", first, sizeof(first)) < 0) return -1;
    if (ask("Last name: ", last, sizeof(last)) < 0) return -1;
    if (ask("Initial password: ", pw, sizeof(pw)) < 0) return -1;
    if (!valid_word(first, 49) || !valid_word(last, 49)) {
        printf("Names must be 1-49 characters, no spaces.\n");
        return 0;
    }
    if (strlen(pw) < 4 || !valid_word(pw, 63)) {
        printf("Password must be 4-63 characters, no spaces.\n");
        return 0;
    }
    snprintf(req, sizeof(req), "ADD_EMPLOYEE %.49s %.49s %.63s\n", first, last, pw);
    if (request(fd, req, reply, sizeof(reply)) < 0) return -1;
    int id;
    if (sscanf(reply, "OK %d", &id) == 1) {
        printf("Employee created. New employee ID: %d\n", id);
    } else {
        printf("Server: %s\n", reply);
    }
    return 0;
}

static int modify_employee_request(int fd) {
    char first[128], last[128], req[MAX_LINE_LEN], reply[MAX_LINE_LEN];
    int eid, r = ask_id("Employee ID: ", &eid);
    if (r <= 0) return r;
    if (ask("New first name: ", first, sizeof(first)) < 0) return -1;
    if (ask("New last name: ", last, sizeof(last)) < 0) return -1;
    if (!valid_word(first, 49) || !valid_word(last, 49)) {
        printf("Names must be 1-49 characters, no spaces.\n");
        return 0;
    }
    snprintf(req, sizeof(req), "MODIFY_EMPLOYEE %d %.49s %.49s\n", eid, first, last);
    if (request(fd, req, reply, sizeof(reply)) < 0) return -1;
    printf("Server: %s\n", reply);
    return 0;
}

static int view_customer_request(int fd) {
    char req[MAX_LINE_LEN], reply[MAX_LINE_LEN], first[64], last[64], status[32];
    int cid, id, r = ask_id("Customer ID: ", &cid);
    float bal, loan;
    if (r <= 0) return r;
    snprintf(req, sizeof(req), "VIEW_CUSTOMER %d\n", cid);
    if (request(fd, req, reply, sizeof(reply)) < 0) return -1;
    if (sscanf(reply, "OK %d %63s %63s %f %f %31s", &id, first, last, &bal, &loan, status) == 6) {
        printf("Customer %d: %s %s | Balance %.2f | Loan %.2f | %s\n",
               id, first, last, bal, loan, status);
    } else {
        printf("Server: %s\n", reply);
    }
    return 0;
}

/* Returns 0 = logged out, -1 = stop. */
static int admin_menu(int fd, int id) {
    char choice[16];
    while (1) {
        printf("\n--- Admin %d ---\n", id);
        printf("1. Add employee\n2. Modify employee\n3. Promote employee to manager\n"
               "4. Demote manager to employee\n5. Delete employee\n6. View customer\n"
               "7. Modify customer\n8. Delete customer\n9. Change password\n10. List staff\n0. Logout\n");
        if (ask("Choice: ", choice, sizeof(choice)) < 0) return -1;
        int r;
        if (strcmp(choice, "1") == 0) {
            r = add_employee_request(fd);
        } else if (strcmp(choice, "2") == 0) {
            r = modify_employee_request(fd);
        } else if (strcmp(choice, "3") == 0) {
            r = id_command(fd, "PROMOTE", "Employee ID: ", "Promoted. New manager ID:");
        } else if (strcmp(choice, "4") == 0) {
            r = id_command(fd, "DEMOTE", "Manager ID: ", "Demoted. New employee ID:");
        } else if (strcmp(choice, "5") == 0) {
            r = id_command(fd, "DELETE_EMPLOYEE", "Employee ID: ", NULL);
        } else if (strcmp(choice, "6") == 0) {
            r = view_customer_request(fd);
        } else if (strcmp(choice, "7") == 0) {
            r = modify_customer_request(fd);
        } else if (strcmp(choice, "8") == 0) {
            r = id_command(fd, "DELETE_CUSTOMER", "Customer ID: ", NULL);
        } else if (strcmp(choice, "9") == 0) {
            r = password_request(fd);
        } else if (strcmp(choice, "10") == 0) {
            r = list_request(fd, "LIST_STAFF", "Staff", "No staff.");
        } else if (strcmp(choice, "0") == 0) {
            return do_logout(fd);
        } else {
            printf("Invalid choice.\n");
            continue;
        }
        if (r < 0) return -1;
    }
}

/* Asks loan ID + employee ID, sends ASSIGN. -1 if the connection broke. */
static int assign_request(int fd) {
    char req[MAX_LINE_LEN], reply[MAX_LINE_LEN];
    int loan_id, emp_id, r;
    if ((r = ask_id("Loan ID: ", &loan_id)) <= 0) return r;
    if ((r = ask_id("Employee ID: ", &emp_id)) <= 0) return r;
    snprintf(req, sizeof(req), "ASSIGN %d %d\n", loan_id, emp_id);
    if (request(fd, req, reply, sizeof(reply)) < 0) return -1;
    printf("Server: %s\n", reply);
    return 0;
}

/* Manager menu. Options 3-6 are wired to server commands added in Steps 4-5. */
static int manager_menu(int fd, int id) {
    char choice[16];
    while (1) {
        printf("\n--- Manager %d ---\n", id);
        printf("1. View pending loans\n2. Assign loan to employee\n"
               "3. Activate customer\n4. Deactivate customer\n"
               "5. View feedback\n6. Mark feedback reviewed\n"
               "7. Change password\n8. View all loans\n0. Logout\n");
        if (ask("Choice: ", choice, sizeof(choice)) < 0) return -1;
        int r = 0;
        if (strcmp(choice, "1") == 0) {
            r = list_request(fd, "PENDING_LOANS", "Pending loans", "No pending loans.");
        } else if (strcmp(choice, "2") == 0) {
            r = assign_request(fd);
        } else if (strcmp(choice, "3") == 0) {
            r = id_command(fd, "ACTIVATE", "Customer ID: ", NULL);
        } else if (strcmp(choice, "4") == 0) {
            r = id_command(fd, "DEACTIVATE", "Customer ID: ", NULL);
        } else if (strcmp(choice, "5") == 0) {
            r = list_request(fd, "FEEDBACK_LIST", "Feedback", "No feedback.");
        } else if (strcmp(choice, "6") == 0) {
            r = id_command(fd, "FEEDBACK_REVIEW", "Feedback ID: ", NULL);
        } else if (strcmp(choice, "7") == 0) {
            r = password_request(fd);
        } else if (strcmp(choice, "8") == 0) {
            r = list_request(fd, "ALL_LOANS", "All loans", "No loans yet.");
        } else if (strcmp(choice, "0") == 0) {
            return do_logout(fd);
        } else {
            printf("Invalid choice.\n");
        }
        if (r < 0) return -1;
    }
}

static int basic_menu(int fd, int role, int id) {
    char choice[16];
    while (1) {
        printf("\n--- %s %d ---\n", role_name(role), id);
        printf("0. Logout\n");
        if (ask("Choice: ", choice, sizeof(choice)) < 0) {
            return -1;
        }
        if (strcmp(choice, "0") == 0) {
            return do_logout(fd);
        }
        printf("Invalid choice.\n");
    }
}

/* Picks the menu for the logged-in role.
   Returns 0 = logged out (back to main menu), -1 = stop. */
static int session_menu(int fd, int role, int id) {
    if (role == 1) {
        return admin_menu(fd, id);
    }
    if (role == 2) {
        return manager_menu(fd, id);
    }
    if (role == 3) {
        return employee_menu(fd, id);
    }
    if (role == 4) {
        return customer_menu(fd, id);
    }
    return basic_menu(fd, role, id);
}

int main(int argc, char* argv[]) {
    const char* host = (argc > 1) ? argv[1] : DEFAULT_HOST;
    int fd = connect_to_server(host);
    if (fd < 0) {
        return 1;
    }

    char line[MAX_LINE_LEN], choice[16];

    /* No welcome within 2 s = server busy (all slots or workers in use).
       The client tells the user itself; the server sends nothing extra. */
    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    if (poll(&pfd, 1, 2000) == 0) {
        printf("Server busy, please wait...\n");
        fflush(stdout);
    }
    if (read_line(fd, line, sizeof(line)) <= 0) {   /* the server's welcome line */
        fprintf(stderr, "Server closed the connection.\n");
        close(fd);
        return 1;
    }
    printf("Connected to server.\n");

    while (1) {
        printf("\n=== Banking System ===\n1. Login\n2. Exit\n");
        if (ask("Choice: ", choice, sizeof(choice)) < 0) {
            break;
        }
        if (strcmp(choice, "1") == 0) {
            int role = 0, id = 0;
            int r = do_login(fd, &role, &id);
            if (r < 0) {
                break;
            }
            if (r == 1 && session_menu(fd, role, id) < 0) {
                break;
            }
        } else if (strcmp(choice, "2") == 0) {
            break;
        } else {
            printf("Invalid choice.\n");
        }
    }

    send_line(fd, "QUIT\n");
    read_line(fd, line, sizeof(line));
    close(fd);
    printf("Goodbye.\n");
    return 0;
}
