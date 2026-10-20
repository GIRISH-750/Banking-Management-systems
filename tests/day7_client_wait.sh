#!/bin/bash
# Day 7: client prints "Server busy" while waiting, then works normally
cd "$(dirname "$0")/.." || exit 1
FAIL=0
./seed test > /dev/null
BANK_POOL=1 ./server > /tmp/day7_wait.log 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; wait $SRV 2>/dev/null' EXIT
sleep 0.3

exec 3<>/dev/tcp/127.0.0.1/5000          # this connection takes the only worker
read -r -t 2 w <&3
printf "2\n" | timeout 10 ./client 3>&- > /tmp/day7_busy.out 2>&1 &
CL=$!
sleep 3
exec 3>&-                                 # free the worker
wait $CL

for t in "Server busy, please wait" "Connected to server" "Goodbye"; do
    if grep -q "$t" /tmp/day7_busy.out; then
        echo "  PASS busy client printed: $t"
    else
        echo "  FAIL busy client did not print: $t"; FAIL=1
    fi
done

printf "2\n" | timeout 10 ./client > /tmp/day7_free.out 2>&1
if grep -q "Server busy" /tmp/day7_free.out; then
    echo "  FAIL free server: client wrongly said busy"; FAIL=1
else
    echo "  PASS free server: no busy message"
fi
[ $FAIL -eq 0 ] && echo "Day 7 client wait: ALL PASS" || { echo "Day 7 client wait: FAILED"; exit 1; }
