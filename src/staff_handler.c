#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "staff_handler.h"
#include "entities.h"
#include "rec_store.h"
#include "lock.h"
#include "hash.h"
#include "auth.h"
#include "net_util.h"

union Staff {
    struct Admin a;
    struct Manager m;
    struct Employee e;
};

/* 0 = ok, 1 = wrong old password, 2 = bad new password, -1 = error */
static int change_password(int role, int id, const char* old_pw, const char* new_pw) {
    union Staff r;
    const char* path;
    size_t size;
    char* stored;
    switch (role) {
        case ROLE_ADMIN:    path = ADMINS_FILE;    size = sizeof(r.a); stored = r.a.password; break;
        case ROLE_MANAGER:  path = MANAGERS_FILE;  size = sizeof(r.m); stored = r.m.password; break;
        case ROLE_EMPLOYEE: path = EMPLOYEES_FILE; size = sizeof(r.e); stored = r.e.password; break;
        default: return -1;
    }
    if (strlen(new_pw) < 4) {
        return 2;
    }
    int fd = rec_open(path);
    if (fd < 0) {
        return -1;
    }
    if (lock_record(fd, id, size, F_WRLCK) < 0) {
        close(fd);
        return -1;
    }
    int rc = -1;
    if (rec_read(fd, id, &r, size) == REC_OK) {
        char h[PASS_LEN];
        hash_password(old_pw, h);
        if (strcmp(h, stored) != 0) {
            rc = 1;
        } else {
            hash_password(new_pw, stored);
            rc = (rec_write(fd, id, &r, size) == REC_OK) ? 0 : -1;
        }
    }
    unlock_record(fd, id, size);
    close(fd);
    return rc;
}

int handle_staff(int fd, int role, int id, const char* line) {
    if (strncmp(line, "PASSWD", 6) != 0) {
        return 0;
    }
    char old_pw[64], new_pw[64], extra;
    if (sscanf(line, "PASSWD %63s %63s %c", old_pw, new_pw, &extra) != 2) {
        send_line(fd, "ERR Usage: PASSWD <old> <new>\n");
        return 1;
    }
    int rc = change_password(role, id, old_pw, new_pw);
    if (rc == 0) {
        send_line(fd, "OK Password changed\n");
    } else if (rc == 1) {
        send_line(fd, "ERR Wrong old password\n");
    } else if (rc == 2) {
        send_line(fd, "ERR New password must be at least 4 characters\n");
    } else {
        send_line(fd, "ERR Internal error\n");
    }
    return 1;
}
