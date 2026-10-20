#!/bin/bash
# Smoke test for the employee module. Run: bash tests/emp_smoke.sh
pkill -x server 2>/dev/null; sleep 0.3
./seed test > /dev/null
./server > /tmp/server.log 2>&1 &
SP=$!
sleep 0.5
exec 3<>/dev/tcp/127.0.0.1/5000
for cmd in \
  "LOGIN 3 1 emp123" \
  "ADD_CUSTOMER Ravi Kumar ravi1234" \
  "MODIFY_CUSTOMER 1 Alicia Rao" \
  "MODIFY_CUSTOMER 999 X Y" \
  "PASSBOOK 999" \
  "LOGOUT" \
  "LOGIN 4 3 ravi1234" \
  "DEPOSIT 500" \
  "ADD_CUSTOMER Hacker Man pass1234" \
  "LOGOUT" \
  "LOGIN 3 1 emp123" \
  "PASSBOOK 3" \
  "QUIT"; do
  printf '%s\n' "$cmd" >&3
  echo ">> $cmd"
done
echo "======== server replies ========"
timeout 2 cat <&3
kill $SP 2>/dev/null
