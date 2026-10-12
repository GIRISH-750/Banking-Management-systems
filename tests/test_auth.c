#include <stdio.h>
#include "auth.h"

static int failed = 0;

static void check(const char* name, int got, int want) {
    if (got == want) {
        printf("PASS  %s\n", name);
    } else {
        printf("FAIL  %s (got %d, want %d)\n", name, got, want);
        failed++;
    }
}

int main(void) {
    check("admin ok",        authenticate(ROLE_ADMIN, 1, "admin123"),     AUTH_OK);
    check("manager ok",      authenticate(ROLE_MANAGER, 1, "manager123"), AUTH_OK);
    check("employee ok",     authenticate(ROLE_EMPLOYEE, 1, "emp123"),    AUTH_OK);
    check("customer 1 ok",   authenticate(ROLE_CUSTOMER, 1, "alice123"),  AUTH_OK);
    check("customer 2 ok",   authenticate(ROLE_CUSTOMER, 2, "bob123"),    AUTH_OK);

    check("wrong password",  authenticate(ROLE_CUSTOMER, 1, "wrong"),    AUTH_WRONG_PASSWORD);
    check("empty password",  authenticate(ROLE_CUSTOMER, 1, ""),         AUTH_WRONG_PASSWORD);
    check("id not present",  authenticate(ROLE_CUSTOMER, 99, "alice123"), AUTH_NO_SUCH_USER);
    check("id zero",         authenticate(ROLE_CUSTOMER, 0, "alice123"),  AUTH_NO_SUCH_USER);
    check("id negative",     authenticate(ROLE_ADMIN, -1, "admin123"),    AUTH_NO_SUCH_USER);
    check("bad role",        authenticate(9, 1, "alice123"),              AUTH_ERROR);
    check("null password",   authenticate(ROLE_CUSTOMER, 1, NULL),        AUTH_ERROR);

    printf(failed ? "\n%d test(s) FAILED\n" : "\nAll tests passed\n", failed);
    return failed ? 1 : 0;
}
