#!/bin/bash
# Smoke test for the admin module. Run: bash tests/admin_smoke.sh
pkill -x server 2>/dev/null; sleep 0.3
./seed > /dev/null
./server > /tmp/server.log 2>&1 &
SP=$!
sleep 0.5
exec 3<>/dev/tcp/127.0.0.1/5000
for cmd in \
  "LOGIN 1 1 admin123" \
  "ADD_EMPLOYEE Ravi Kumar ravi1234" \
  "MODIFY_EMPLOYEE 2 Ravi Sharma" \
  "DEMOTE 1" \
  "PROMOTE 2" \
  "PROMOTE 2" \
  "DEMOTE 1" \
  "DEMOTE 2" \
  "VIEW_CUSTOMER 1" \
  "DELETE_CUSTOMER 999" \
  "DELETE_EMPLOYEE 1" \
  "PASSWD admin123 newpass1" \
  "LOGOUT" \
  "LOGIN 2 2 ravi1234" \
  "PASSWD ravi1234 mgrpass1" \
  "LOGOUT" \
  "LOGIN 3 2 ravi1234" \
  "LOGIN 3 1 emp123" \
  "LOGIN 1 1 newpass1" \
  "QUIT"; do
  printf '%s\n' "$cmd" >&3
  echo ">> $cmd"
done
echo "======== server replies ========"
timeout 2 cat <&3
kill $SP 2>/dev/null
