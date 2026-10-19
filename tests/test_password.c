/* Run ./seed first. Alice (id 1) starts with password alice123. */
#include <stdio.h>
#include "customer_ops.h"
#include "auth.h"

static int passed = 0, total = 0;

static void check(int cond, const char* name) {
    total++;
    if (cond) {
        passed++;
        printf("PASS  %s\n", name);
    } else {
        printf("FAIL  %s\n", name);
    }
}

int main(void) {
    check(op_change_password(1, "wrong", "newpass1") == OP_WRONG_PASSWORD, "wrong old password rejected");
    check(op_change_password(1, "alice123", "abc") == OP_BAD_PASSWORD, "too short new password rejected");
    check(op_change_password(1, "alice123", "alice123") == OP_BAD_PASSWORD, "same as old rejected");
    check(authenticate(ROLE_CUSTOMER, 1, "alice123") == AUTH_OK, "old password still works after failures");

    check(op_change_password(1, "alice123", "newpass1") == OP_OK, "change alice123 -> newpass1");
    check(authenticate(ROLE_CUSTOMER, 1, "newpass1") == AUTH_OK, "login with NEW password works");
    check(authenticate(ROLE_CUSTOMER, 1, "alice123") != AUTH_OK, "login with OLD password fails");
    check(authenticate(ROLE_CUSTOMER, 2, "bob123") == AUTH_OK, "Bob not affected");

    op_change_password(1, "newpass1", "alice123");   /* put it back */

    printf("%d/%d passed\n", passed, total);
    return passed == total ? 0 : 1;
}
