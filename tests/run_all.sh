#!/bin/bash
# Builds and runs every unit/concurrency test. Each test starts from fresh data.
# Run from the project folder: bash tests/run_all.sh
set -u
CFLAGS="-Wall -Wextra -Iinclude"
CORE="src/customer_ops.c src/customer_store.c src/lock.c src/txn_store.c src/counter.c src/hash.c"
fail=0

gcc $CFLAGS src/hash.c src/seed.c -o seed -lcrypto || exit 1

run() {   # run <name> <extra sources...>
    local name=$1; shift
    ./seed > /dev/null
    if gcc $CFLAGS tests/$name.c "$@" -o $name -pthread -lcrypto 2> /tmp/build_$name.log; then
        if ./$name > /tmp/out_$name.log 2>&1; then
            echo "PASS  $name   ($(tail -1 /tmp/out_$name.log))"
        else
            echo "FAIL  $name   (see /tmp/out_$name.log)"; fail=1
        fi
    else
        echo "FAIL  $name   (does not compile, see /tmp/build_$name.log)"; fail=1
    fi
}

run test_lock           src/lock.c
run test_customer_store src/customer_store.c src/lock.c
run test_customer_ops   $CORE
run test_transfer       $CORE
run test_history        $CORE
run test_password       src/auth.c $CORE

./seed > /dev/null
[ $fail -eq 0 ] && echo "ALL TESTS PASSED" || echo "SOME TESTS FAILED"
exit $fail
