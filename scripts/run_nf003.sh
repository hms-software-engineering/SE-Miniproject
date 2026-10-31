#!/usr/bin/env bash
# Usage: PASS='<password>' TOKEN_USER=f024_receptionist SERVER_PID=<pid> ./scripts/run_nf003.sh
# Optional: SLOTQUERY='doctorId=12&date=2026-11-02' DURATION=1800
set -euo pipefail
: "${PASS:?Set PASS to the test account password}"
: "${SERVER_PID:?Set SERVER_PID (pgrep hms_server)}"
USER_NAME="${TOKEN_USER:-f024_receptionist}"
HOST="${HOST:-localhost}"; PORT="${PORT:-8443}"; DURATION="${DURATION:-1800}"
SLOTQUERY="${SLOTQUERY:-doctorId=12&date=2026-11-02}"
D=tests/performance; OUT=results/nf003
rm -rf "$OUT"; mkdir -p "$OUT"
kill -0 "$SERVER_PID" || { echo "Server not running"; exit 1; }

( while kill -0 "$SERVER_PID" 2>/dev/null; do
    echo "$(date +%T) cpu/mem: $(ps -o %cpu=,rss= -p "$SERVER_PID")" >> "$OUT/server_monitor.log"
    sleep 10
  done; echo "SERVER DIED $(date +%T)" >> "$OUT/server_monitor.log" ) &
MON=$!

jmeter -n -t $D/NF-003-concurrent-sessions.jmx \
  -Jhost="$HOST" -Jport="$PORT" -Juser="$USER_NAME" -Jpass="$PASS" \
  -Jduration="$DURATION" -Jslotquery="$SLOTQUERY" \
  -l "$OUT/results.jtl" -e -o "$OUT/report"
kill "$MON" 2>/dev/null || true

ALIVE=yes; kill -0 "$SERVER_PID" 2>/dev/null || ALIVE=no
grep -q "SERVER DIED" "$OUT/server_monitor.log" && ALIVE=no

python3 - "$OUT/results.jtl" "$ALIVE" "$DURATION" <<'EOF' | tee tests/performance/NF-003-summary.txt
import csv, sys, math
rows = list(csv.DictReader(open(sys.argv[1])))
alive, dur = sys.argv[2], sys.argv[3]
el = sorted(int(r["elapsed"]) for r in rows)
n = len(el); f = sum(1 for r in rows if r["success"] != "true")
rate = 100 * f / n
p95 = el[math.ceil(0.95 * n) - 1]
mx = max(int(r["allThreads"]) for r in rows)
ok = rate < 1.0 and alive == "yes" and mx >= 200
print("NF-003 / TC-PERF-03 summary")
print(f"Peak concurrent sessions: {mx}   Duration: {dur}s")
print(f"Total: {n}  Successful: {n-f}  Failed: {f}  Error rate: {rate:.3f}%")
print(f"Avg: {sum(el)/n:.0f} ms  p95: {p95} ms  max: {el[-1]} ms")
print(f"Server survived: {alive}")
print("RESULT:", "PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
EOF
cp "$OUT/results.jtl" $D/NF-003-results.jtl