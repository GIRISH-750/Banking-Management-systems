#!/bin/bash
# Day 4 automated checks (employee + admin + break-it). Run: bash tests/day4_check.sh
cd "$(dirname "$0")/.." || exit 1
pass=0; fail=0

start_server() {
    pkill -x server 2>/dev/null; sleep 0.3
    ./seed test > /dev/null
    ./server > /tmp/server.log 2>&1 &
    SP=$!
    sleep 0.5
}

# run "<cmd>" "<cmd>" ... : one connection, sends all commands + QUIT, prints replies
run() {
    exec 3<>/dev/tcp/127.0.0.1/5000
    for c in "$@" "QUIT"; do
        printf '%s\n' "$c" >&3
    done
    timeout 3 cat <&3 | tail -n +2
    exec 3>&-
}

# check <name> <pattern> <actual output>
check() {
    if grep -q -- "$2" <<< "$3"; then
        echo "PASS  $1"; pass=$((pass + 1))
    else
        echo "FAIL  $1   (got: $(echo "$3" | tr '\n' '|'))"; fail=$((fail + 1))
    fi
}

emp_pw() { if [ "$1" = 1 ]; then echo emp123; else echo "pw$1$1$1$1"; fi; }

start_server

out=$(run "LOGIN 3 1 emp123" "ADD_CUSTOMER Ravi Kumar ravi1234")
check "employee adds customer -> ID 3" "^OK 3$" "$out"
check "new customer can log in" "OK Login" "$(run "LOGIN 4 3 ravi1234")"

check "customer cannot run employee command" "ERR Unknown command" \
      "$(run "LOGIN 4 1 alice123" "ADD_CUSTOMER X Y pass1234")"
check "customer cannot run admin command" "ERR Unknown command" \
      "$(run "LOGIN 4 1 alice123" "DELETE_CUSTOMER 2")"
check "employee cannot run admin command" "ERR Unknown command" \
      "$(run "LOGIN 3 1 emp123" "PROMOTE 1")"

check "delete customer with balance refused" "ERR Balance must be 0" \
      "$(run "LOGIN 1 1 admin123" "DELETE_CUSTOMER 1")"
check "delete customer with 0 balance" "OK Customer deleted" \
      "$(run "LOGIN 1 1 admin123" "DELETE_CUSTOMER 3")"
check "deleted customer cannot log in" "ERR Account is inactive" \
      "$(run "LOGIN 4 3 ravi1234")"
check "cannot demote the last manager" "ERR Cannot demote the last" \
      "$(run "LOGIN 1 1 admin123" "DEMOTE 1")"

# admin adds employees 2..10 (needed for parallel tests: one session per user)
cmds=("LOGIN 1 1 admin123")
for i in $(seq 2 10); do cmds+=("ADD_EMPLOYEE E$i L$i $(emp_pw $i)"); done
run "${cmds[@]}" > /dev/null

# BREAK-IT 1: 10 employees add a customer at the same moment
for i in $(seq 1 10); do
    run "LOGIN 3 $i $(emp_pw $i)" "ADD_CUSTOMER P$i Q$i pass1234" > /tmp/add_$i.txt &
done
for p in $(jobs -p); do [ "$p" != "$SP" ] && wait "$p"; done
uniq_ids=$(cat /tmp/add_*.txt | grep -E '^OK [0-9]+$' | sort -u | wc -l)
check "10 parallel adds -> 10 unique IDs" "^10$" "$uniq_ids"

# BREAK-IT 2: 10 employees rename the same customer at the same moment
for i in $(seq 1 10); do
    run "LOGIN 3 $i $(emp_pw $i)" "MODIFY_CUSTOMER 2 Fn$i Ln$i" > /dev/null &
done
for p in $(jobs -p); do [ "$p" != "$SP" ] && wait "$p"; done
check "parallel renames never mix first/last name" 'OK 2 Fn\([0-9]*\) Ln\1 ' \
      "$(run "LOGIN 1 1 admin123" "VIEW_CUSTOMER 2")"

check "promote employee 2 -> manager 2" "^OK 2$" \
      "$(run "LOGIN 1 1 admin123" "PROMOTE 2")"
check "promoted user logs in as manager with same password" "OK Login" \
      "$(run "LOGIN 2 2 pw2222")"
check "promoting an inactive employee refused" "ERR User is not active" \
      "$(run "LOGIN 1 1 admin123" "PROMOTE 2")"
check "wrong old password refused" "ERR Wrong old password" \
      "$(run "LOGIN 3 1 emp123" "PASSWD wrongpw newpw123")"

kill $SP 2>/dev/null; wait $SP 2>/dev/null
echo "---------------------------------"
echo "Day 4 checks: $pass passed, $fail failed"
[ $fail -eq 0 ]
