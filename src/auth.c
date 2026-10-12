#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "auth.h"
#include "hash.h"
#include "entities.h"

#define LOAD_OK        0
#define LOAD_NOT_FOUND 1
#define LOAD_ERROR    -1

/* Reads the record for `id` (stored at position id-1) into buf. */
static int load_record(const char* path, int id, void* buf, size_t size) {
    if (id < 1) {
        return LOAD_NOT_FOUND;
    }
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return LOAD_ERROR;
    }
    ssize_t n = pread(fd, buf, size, (off_t)(id - 1) * (off_t)size);
    close(fd);
    if (n < 0) {
        return LOAD_ERROR;
    }
    if ((size_t)n < size) {
        return LOAD_NOT_FOUND;
    }
    return LOAD_OK;
}

static int password_matches(const char* stored, const char* given) {
    char hashed[HASH_HEX_LEN];
    hash_password(given, hashed);
    return strncmp(stored, hashed, PASS_LEN) == 0;
}

int authenticate(int role, int id, const char* password) {
    int rc;

    if (password == NULL) {
        return AUTH_ERROR;
    }

    if (role == ROLE_ADMIN) {
        struct Admin a;
        rc = load_record("data/admins.bin", id, &a, sizeof(a));
        if (rc == LOAD_ERROR) {
            return AUTH_ERROR;
        }
        if (rc == LOAD_NOT_FOUND || a.adminID != id) {
            return AUTH_NO_SUCH_USER;
        }
        if (!password_matches(a.password, password)) {
            return AUTH_WRONG_PASSWORD;
        }
        return AUTH_OK;
    }

    if (role == ROLE_MANAGER) {
        struct Manager m;
        rc = load_record("data/managers.bin", id, &m, sizeof(m));
        if (rc == LOAD_ERROR) {
            return AUTH_ERROR;
        }
        if (rc == LOAD_NOT_FOUND || m.managerID != id) {
            return AUTH_NO_SUCH_USER;
        }
        if (!password_matches(m.password, password)) {
            return AUTH_WRONG_PASSWORD;
        }
        return AUTH_OK;
    }

    if (role == ROLE_EMPLOYEE) {
        struct Employee e;
        rc = load_record("data/employees.bin", id, &e, sizeof(e));
        if (rc == LOAD_ERROR) {
            return AUTH_ERROR;
        }
        if (rc == LOAD_NOT_FOUND || e.employeeID != id) {
            return AUTH_NO_SUCH_USER;
        }
        if (!password_matches(e.password, password)) {
            return AUTH_WRONG_PASSWORD;
        }
        if (strcmp(e.status, "ACTIVE") != 0) {
            return AUTH_INACTIVE;
        }
        return AUTH_OK;
    }

    if (role == ROLE_CUSTOMER) {
        struct Customer c;
        rc = load_record("data/customers.bin", id, &c, sizeof(c));
        if (rc == LOAD_ERROR) {
            return AUTH_ERROR;
        }
        if (rc == LOAD_NOT_FOUND || c.userID != id) {
            return AUTH_NO_SUCH_USER;
        }
        if (!password_matches(c.password, password)) {
            return AUTH_WRONG_PASSWORD;
        }
        if (strcmp(c.status, "ACTIVE") != 0) {
            return AUTH_INACTIVE;
        }
        return AUTH_OK;
    }

    return AUTH_ERROR;
}
