#!/bin/bash
# Day 6 crash-recovery checks (atomicity + durability). Run: bash tests/day6_crash.sh
cd "$(dirname "$0")/.." || exit 1
pass=0; fail=0
fresh_data() {
    pkill -x server 2>/dev/null; sleep 0.3
    rm -f data/loans.bin data/feedback.bin data/journal.bin
    ./seed > /dev/null
}
start() { BANK_CRASH="$1" ./server > /tmp/server.log 2>&1 & SP=$!; sleep 0.5; }
stop() { kill $SP 2>/dev/null; wait $SP 2>/dev/null; }
run() {
    exec 3<>/dev/tcp/127.0.0.1/5000
    for c in "$@" "QUIT"; do printf '%s\n' "$c" >&3; done
    timeout 3 cat <&3 | tail -n +2
    exec 3>&-
}
check() {
    if grep -q -- "$2" <<< "$3"; then echo "PASS  $1"; pass=$((pass + 1))
    else echo "FAIL  $1   (got: $(echo "$3" | tr '\n' '|'))"; fail=$((fail + 1)); fi
}
crashed() {   # <name>: server must have died at that crash point, journal non-empty
    sleep 0.3
    check "server died at '$1' crash point" "simulated crash at '$1'" "$(cat /tmp/server.log)"
    check "journal holds the unfinished entry" "^yes$" "$([ -s data/journal.bin ] && echo yes)"
    start ""
    check "restart rolled back 1 operation" "rolled back 1" "$(cat /tmp/server.log)"
    check "journal emptied after recovery" "^yes$" "$([ ! -s data/journal.bin ] && echo yes)"
}
C1="LOGIN 4 1 alice123"; C2="LOGIN 4 2 bob123"
M="LOGIN 2 1 manager123"; E="LOGIN 3 1 emp123"; A="LOGIN 1 1 admin123"

bash build.sh > /dev/null || { echo "build failed"; exit 1; }
fresh_data

echo "== crash in the middle of a transfer =="
start transfer
run "$C1" "TRANSFER 2 1000" > /dev/null
crashed transfer
check "customer 1 money back (5000)" "Balance: 5000.00" "$(run "$C1" "BALANCE")"
check "customer 2 unchanged (10000)" "Balance: 10000.00" "$(run "$C2" "BALANCE")"
check "no half transfer in history" "^OK 0$" "$(run "$C1" "HISTORY")"
check "transfer works after recovery" "Balance: 4000.00" "$(run "$C1" "TRANSFER 2 1000")"
stop

echo "== crash in the middle of a loan approval =="
start ""
run "$C1" "LOAN_APPLY 3000" > /dev/null
run "$M" "ASSIGN 1 1" > /dev/null
stop
start approve
run "$E" "APPROVE 1" > /dev/null
crashed approve
check "loan back to ASSIGNED" "ASSIGNED" "$(run "$C1" "MY_LOANS")"
check "money not credited (4000)" "Balance: 4000.00" "$(run "$C1" "BALANCE")"
check "approval works after recovery" "approved" "$(run "$E" "APPROVE 1")"
check "money credited exactly once (7000)" "Balance: 7000.00" "$(run "$C1" "BALANCE")"
stop

echo "== crash in the middle of promote =="
start promote
run "$A" "PROMOTE 1" > /dev/null
crashed promote
check "employee 1 still ACTIVE (can log in)" "OK Login" "$(run "$E")"
check "no half-created manager 2" "ERR" "$(run "LOGIN 2 2 emp123")"
check "promote works after recovery" "^OK [0-9]" "$(run "$A" "PROMOTE 1")"
stop

echo "== crash in the middle of demote =="
start demote
run "$A" "DEMOTE 1" > /dev/null
crashed demote
check "manager 1 still ACTIVE (can log in)" "OK Login" "$(run "$M")"
check "no half-created employee 2" "ERR" "$(run "LOGIN 3 2 manager123")"
check "demote works after recovery" "^OK [0-9]" "$(run "$A" "DEMOTE 1")"
stop

fresh_data
echo "--------------------------------"
echo "Day 6 crash checks: $pass passed, $fail failed"
