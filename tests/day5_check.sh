#!/bin/bash
# Day 5 automated checks (loan flow, manager, feedback + break-it). Run: bash tests/day5_check.sh
cd "$(dirname "$0")/.." || exit 1
pass=0; fail=0

fresh_data() {
    pkill -x server 2>/dev/null; sleep 0.3
    rm -f data/loans.bin data/feedback.bin     # seed does not reset these (fix on Day 6)
    ./seed > /dev/null
}
start_server() {
    fresh_data
    ./server > /tmp/server.log 2>&1 &
    SP=$!
    sleep 0.5
}
run() {
    exec 3<>/dev/tcp/127.0.0.1/5000
    for c in "$@" "QUIT"; do printf '%s\n' "$c" >&3; done
    timeout 3 cat <&3 | tail -n +2
    exec 3>&-
}
check() {
    if grep -q -- "$2" <<< "$3"; then
        echo "PASS  $1"; pass=$((pass + 1))
    else
        echo "FAIL  $1   (got: $(echo "$3" | tr '\n' '|'))"; fail=$((fail + 1))
    fi
}
bal() { run "LOGIN 4 $1 $2" "BALANCE" | grep -o 'Balance: [0-9.]*' | awk '{print $2}'; }

C1="LOGIN 4 1 alice123"; C2="LOGIN 4 2 bob123"
M="LOGIN 2 1 manager123"; E="LOGIN 3 1 emp123"; A="LOGIN 1 1 admin123"

bash build.sh > /dev/null || { echo "build failed"; exit 1; }
start_server

echo "== loan flow =="
check "check before apply says OK" "OK You can apply" "$(run "$C1" "LOAN_CHECK")"
check "apply -> loan 1 PENDING" "OK Loan 1 applied" "$(run "$C1" "LOAN_APPLY 5000")"
check "second apply refused" "already have an open loan" "$(run "$C1" "LOAN_APPLY 100")"
check "LOAN_CHECK refuses early" "already have an open loan" "$(run "$C1" "LOAN_CHECK")"
check "negative amount refused" "Invalid amount" "$(run "$C2" "LOAN_APPLY -5")"
check "text amount refused" "Usage" "$(run "$C2" "LOAN_APPLY abc")"
check "too large refused" "Invalid amount" "$(run "$C2" "LOAN_APPLY 1e40")"
check "manager sees pending loan" "Loan 1 .*customer 1" "$(run "$M" "PENDING_LOANS")"
check "assign to missing employee" "Employee not found" "$(run "$M" "ASSIGN 1 99")"
check "customer cannot assign" "ERR" "$(run "$C1" "ASSIGN 1 1")"
check "assign loan 1 to employee 1" "OK Loan 1 assigned" "$(run "$M" "ASSIGN 1 1")"
check "double assign refused" "right status" "$(run "$M" "ASSIGN 1 1")"
check "employee sees assigned loan" "Loan 1 .*customer 1" "$(run "$E" "MY_ASSIGNED")"
before=$(bal 1 alice123)
check "approve loan 1" "approved, amount credited" "$(run "$E" "APPROVE 1")"
check "double approve refused" "right status" "$(run "$E" "APPROVE 1")"
after=$(bal 1 alice123)
check "balance +5000 after approval" "^1$" "$(awk -v a="$after" -v b="$before" 'BEGIN{print (a-b>4999.99 && a-b<5000.01)}')"
check "history shows LOAN_CREDIT" "LOAN_CREDIT" "$(run "$C1" "HISTORY")"
check "customer sees APPROVED" "APPROVED" "$(run "$C1" "MY_LOANS")"
check "manager all-loans view" "APPROVED .*employee 1" "$(run "$M" "ALL_LOANS")"

echo "== reject + inactive =="
run "$C2" "LOAN_APPLY 700" > /dev/null
run "$M" "ASSIGN 2 1" > /dev/null
check "employee rejects loan 2" "OK Loan 2 rejected" "$(run "$E" "REJECT 2")"
check "customer sees REJECTED" "REJECTED" "$(run "$C2" "MY_LOANS")"
check "can apply again after reject" "OK Loan 3 applied" "$(run "$C2" "LOAN_APPLY 800")"
run "$M" "ASSIGN 3 1" > /dev/null
check "deactivate customer 2" "now INACTIVE" "$(run "$M" "DEACTIVATE 2")"
check "deactivate again -> already" "already in that status" "$(run "$M" "DEACTIVATE 2")"
check "approve for inactive customer refused" "inactive" "$(run "$E" "APPROVE 3")"
check "activate customer 2" "now ACTIVE" "$(run "$M" "ACTIVATE 2")"
check "approve works after activation" "approved" "$(run "$E" "APPROVE 3")"

echo "== deleted account =="
run "$E" "ADD_CUSTOMER Ravi Kumar ravi1234" > /dev/null
run "$A" "DELETE_CUSTOMER 3" > /dev/null
check "activate deleted account refused" "deleted" "$(run "$M" "ACTIVATE 3")"
check "deactivate deleted account refused" "deleted" "$(run "$M" "DEACTIVATE 3")"

echo "== feedback =="
check "customer gives feedback" "OK Feedback 1 submitted" "$(run "$C1" "FEEDBACK Great service")"
check "manager sees feedback" "Great service" "$(run "$M" "FEEDBACK_LIST")"
check "manager marks reviewed" "marked REVIEWED" "$(run "$M" "FEEDBACK_REVIEW 1")"
check "review twice refused" "already reviewed" "$(run "$M" "FEEDBACK_REVIEW 1")"
check "customer cannot read feedback list" "ERR" "$(run "$C1" "FEEDBACK_LIST")"

kill $SP 2>/dev/null; wait $SP 2>/dev/null

echo "== break-it (threads, no server) =="
fresh_data
SRC=$(ls src/*.c | grep -v -e 'server.c' -e 'client.c' -e 'seed.c')
if gcc -Wall -Iinclude tests/test_loan_race.c $SRC -o /tmp/test_loan_race -pthread -lcrypto -lm 2> /tmp/race_build.log; then
    /tmp/test_loan_race
    rc=$?
else
    echo "FAIL  race test did not compile (see /tmp/race_build.log)"; rc=1
fi
fresh_data
echo "--------------------------------"
echo "Day 5 server checks: $pass passed, $fail failed"
[ $fail -eq 0 ] && [ $rc -eq 0 ] && echo "ALL DAY 5 CHECKS PASSED"
