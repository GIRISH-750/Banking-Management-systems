#!/bin/bash
# Tries to break the running server the way a viva examiner would:
# raw commands that skip our client, wrong roles, double login, bad input.
# Needs: nc (netcat). Run from the project folder: bash tests/break_it.sh
set -u
PORT=5000
pass=0; total=0

# build fresh copies of seed and server, then start from clean data
gcc -Wall -Wextra -Iinclude src/hash.c src/seed.c -o seed -lcrypto || exit 1
gcc -Wall -Wextra -Iinclude src/server.c src/auth.c src/hash.c src/net_util.c src/session.c \
    src/customer_handler.c src/customer_ops.c src/customer_store.c src/lock.c \
    src/txn_store.c src/counter.c -o server -pthread -lcrypto || exit 1
./seed > /dev/null
./server > /tmp/break_server.log 2>&1 &
SERVER=$!
sleep 0.5

# send <lines...> : one connection, sends the lines, prints all replies
send() {
    { printf '%s\n' "$@"; sleep 0.4; } | nc -q1 127.0.0.1 $PORT 2>/dev/null
}

# expect <name> <text the reply must contain> <lines...>
expect() {
    local name=$1 want=$2; shift 2
    total=$((total+1))
    if send "$@" | grep -qF -- "$want"; then
        pass=$((pass+1)); echo "PASS  $name"
    else
        echo "FAIL  $name   (wanted: $want)"
    fi
}

expect "command before login refused"      "ERR Unknown command"        "BALANCE"
expect "wrong password refused"            "ERR Invalid ID or password" "LOGIN 4 1 nope"
expect "unknown user, same message"        "ERR Invalid ID or password" "LOGIN 4 999 x"
expect "negative deposit refused"          "ERR Invalid amount"         "LOGIN 4 1 alice123" "DEPOSIT -5"
expect "zero withdraw refused"             "ERR Invalid amount"         "LOGIN 4 1 alice123" "WITHDRAW 0"
expect "junk after amount refused"         "ERR Usage"                  "LOGIN 4 1 alice123" "DEPOSIT 10 junk"
expect "text instead of amount refused"    "ERR Usage"                  "LOGIN 4 1 alice123" "DEPOSIT abc"
expect "overdraw refused"                  "ERR Insufficient balance"   "LOGIN 4 1 alice123" "WITHDRAW 999999"
expect "transfer to self refused"          "ERR Cannot transfer"        "LOGIN 4 1 alice123" "TRANSFER 1 10"
expect "transfer to unknown refused"       "ERR Account not found"      "LOGIN 4 1 alice123" "TRANSFER 999 10"
expect "admin cannot use customer command" "ERR Unknown command"        "LOGIN 1 1 admin123" "BALANCE"
expect "balance unchanged after all that"  "OK Balance: 5000.00"        "LOGIN 4 1 alice123" "BALANCE"

# double login: keep one session open in the background, try a second one
total=$((total+1))
{ printf 'LOGIN 4 2 bob123\n'; sleep 2; } | nc -q1 127.0.0.1 $PORT > /dev/null 2>&1 &
sleep 0.5
if send "LOGIN 4 2 bob123" | grep -qF "ERR User already logged in"; then
    pass=$((pass+1)); echo "PASS  second login of the same user refused"
else
    echo "FAIL  second login of the same user refused"
fi
wait $! 2>/dev/null

# after the first connection dropped (no LOGOUT), Bob must be able to log in again
expect "session freed when client disconnects" "OK Login successful" "LOGIN 4 2 bob123"

# Alice and Bob transfer to each other at the same time (the deadlock case, over the network)
total=$((total+1))
alice=("LOGIN 4 1 alice123"); bob=("LOGIN 4 2 bob123")
for i in $(seq 1 50); do alice+=("TRANSFER 2 1"); bob+=("TRANSFER 1 1"); done
send "${alice[@]}" > /dev/null & p1=$!
send "${bob[@]}" > /dev/null & p2=$!
wait $p1 $p2          # not a bare "wait": that would also wait for the server
a=$(send "LOGIN 4 1 alice123" "BALANCE" | grep -o 'Balance: [0-9.]*')
b=$(send "LOGIN 4 2 bob123" "BALANCE" | grep -o 'Balance: [0-9.]*')
if [ "$a" = "Balance: 5000.00" ] && [ "$b" = "Balance: 10000.00" ]; then
    pass=$((pass+1)); echo "PASS  50 transfers each way at once: no deadlock, no money lost"
else
    echo "FAIL  crossed transfers ended with Alice $a, Bob $b"
fi

kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null
./seed > /dev/null
echo "$pass/$total passed"
