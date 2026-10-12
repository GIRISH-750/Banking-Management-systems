#ifndef HASH_H
#define HASH_H

/* A SHA-256 hash as hex text is 64 characters, plus 1 for the '\0'. */
#define HASH_HEX_LEN 65

/* Writes the SHA-256 hex digest of `plain` into `out`.
   `out` must have room for HASH_HEX_LEN bytes. */
void hash_password(const char* plain, char* out);

#endif
