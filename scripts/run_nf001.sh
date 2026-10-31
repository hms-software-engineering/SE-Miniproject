#!/usr/bin/env bash
# Usage: TOKEN=<bearer> HOST=localhost PORT=8080 ./scripts/run_nf001.sh
set -euo pipefail
: "${TOKEN:?Set TOKEN to a valid test bearer token}"
HOST="${HOST:-localhost}"; PORT="${PORT:-8443}"; PROTOCOL="${PROTOCOL:-https}"
OUT="results/nf001"; rm -rf "$OUT"; mkdir -p "$OUT"
[ -f tests/performance/search_queries.csv ] || { echo "Run the seeder first"; exit 1; }

psql -d hms -c "SELECT COUNT(*) FROM patients" | tee "$OUT/patient_count.txt"

jmeter -n -t tests/performance/NF-001-patient-search.jmx \
  -Jhost="$HOST" -Jport="$PORT" -Jprotocol="$PROTOCOL" -Jtoken="$TOKEN" \
  -l "$OUT/results.jtl" -e -o "$OUT/report"

python3 - "$OUT/results.jtl" <<'EOF'
import csv, sys, math
rows = list(csv.DictReader(open(sys.argv[1])))
el = sorted(int(r["elapsed"]) for r in rows)
fails = sum(1 for r in rows if r["success"] != "true")
p95 = el[math.ceil(0.95 * len(el)) - 1]
print(f"Requests: {len(el)}  Failures: {fails} ({100*fails/len(el):.2f}%)")
print(f"Avg: {sum(el)/len(el):.0f} ms   p95: {p95} ms   max: {el[-1]} ms")
ok = p95 <= 2000 and fails == 0
print("TC-PERF-01:", "PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
EOF