#ifndef AUTH_H
#define AUTH_H

enum Role {
    ROLE_ADMIN = 1,
    ROLE_MANAGER = 2,
    ROLE_EMPLOYEE = 3,
    ROLE_CUSTOMER = 4
};

#define AUTH_OK             0
#define AUTH_NO_SUCH_USER   1
#define AUTH_WRONG_PASSWORD 2
#define AUTH_INACTIVE       3
#define AUTH_ERROR         -1

/* password must be a NUL-terminated string. */
int authenticate(int role, int id, const char* password);

#endif
