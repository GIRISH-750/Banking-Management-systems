#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include "hash.h"

void hash_password(const char* plain, char* out) {
    unsigned char digest[SHA256_DIGEST_LENGTH];

    SHA256((const unsigned char*)plain, strlen(plain), digest);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(out + i * 2, "%02x", digest[i]);
    }
    out[HASH_HEX_LEN - 1] = '\0';
}
