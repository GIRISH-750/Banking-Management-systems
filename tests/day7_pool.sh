#!/bin/bash
# Day 7: thread pool checks (queueing, reuse, 100-connection limit)
cd "$(dirname "$0")/.." || exit 1

FAIL=0
SRV=""

stop_server() {
    if [ -n "$SRV" ]; then
        kill "$SRV" 2>/dev/null
        wait "$SRV" 2>/dev/null
        SRV=""
    fi
}
trap stop_server EXIT

check_log() {   # $1 = log file, $2 = text, $3 = name
    if grep -q "$2" "$1"; then
        echo "  PASS $3"
    else
        echo "  FAIL $3"
        FAIL=$((FAIL + 1))
    fi
}

run_case() {   # $1 = BANK_POOL ("" = default), $2 = mode
    ./seed test > /dev/null
    BANK_POOL="$1" ./server > "/tmp/day7_$2.log" 2>&1 &
    SRV=$!
    sleep 0.3
    if ! kill -0 "$SRV" 2>/dev/null; then
        echo "  FAIL server did not start (port 5000 busy?)"
        cat "/tmp/day7_$2.log"
        FAIL=$((FAIL + 1))
        SRV=""
        return
    fi
    python3 tests/day7_pool_client.py "$2"
    FAIL=$((FAIL + $?))
}

run_case 2 small
check_log /tmp/day7_small.log "pool of 2 workers" "server started 2 workers (BANK_POOL=2)"
check_log /tmp/day7_small.log "client queued" "server log shows queued client"
stop_server

run_case "" full
check_log /tmp/day7_full.log "pool of 100 workers" "default pool = 100 workers"
stop_server

if [ "$FAIL" -eq 0 ]; then
    echo "Day 7 pool: ALL PASS"
else
    echo "Day 7 pool: $FAIL FAILED"
    exit 1
fi
