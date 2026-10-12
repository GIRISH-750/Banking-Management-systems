#include <stdio.h>
#include <string.h>
#include "hash.h"

int main(void) {
    char out[HASH_HEX_LEN];
    const char* expected =
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

    hash_password("abc", out);
    printf("got:      %s\n", out);
    printf("expected: %s\n", expected);

    if (strcmp(out, expected) == 0) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
