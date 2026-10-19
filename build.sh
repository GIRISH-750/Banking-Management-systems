#!/bin/bash
# Builds seed, server and client from current sources. Run: bash build.sh
set -e
CFLAGS="-Wall -Wextra -Iinclude"
gcc $CFLAGS src/hash.c src/seed.c -o seed -lcrypto
SRV=$(ls src/*.c | grep -v -e 'client.c' -e 'seed.c')
gcc $CFLAGS $SRV -o server -pthread -lcrypto
gcc $CFLAGS src/client.c src/net_util.c -o client -pthread
echo "build OK"
