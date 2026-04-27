#!/usr/bin/env bash
# Memory footprint comparison: Redis vs Oktoplus on the same workload.
#
# For each (value_size, key_count) pair:
#   - start a fresh server
#   - record baseline RSS
#   - load N keys (one SET per key, fixed-size value)
#   - record steady-state RSS
#   - delete every key (FLUSHDB)
#   - record post-flush RSS (allocator-retained / fragmentation signal)
#
# Output: a markdown table at benchmark_results/memory_results.md plus
# raw lines under benchmark_results/raw/memory_*.csv.
#
# Run:    bash benchmark_results/run_memory.sh
# Env:    KEY_COUNTS, VALUE_SIZES (space-separated lists) to override defaults.

set -u

CLI=~/git_store/redis/src/redis-cli
BENCH=~/git_store/redis/src/redis-benchmark
REDIS_SERVER=~/git_store/redis/src/redis-server
OKTO_BIN=~/git_store/oktoplus/build/optimized/src/Executables/Oktoplus/oktoplus
OKTO_CONFIG=/tmp/oktoplus_bench.json
RESULTS_DIR=~/git_store/oktoplus/benchmark_results
LOG_DIR=$RESULTS_DIR/server_logs
RAW_DIR=$RESULTS_DIR/raw
REDIS_PORT=6380
OKTO_PORT=6379

KEY_COUNTS=${KEY_COUNTS:-"100000 1000000"}
VALUE_SIZES=${VALUE_SIZES:-"3 64 256 1024"}

mkdir -p "$RAW_DIR" "$LOG_DIR"

log() { echo "[$(date '+%H:%M:%S')] $*"; }

ensure_okto_config() {
    # Always (re)write the config so a stale cache from an older run
    # (e.g. one that enabled gRPC) does not contaminate the measurement.
    # gRPC is omitted on purpose: this benchmark targets the RESP-only
    # deployment, which is the default since oktoplus made gRPC opt-in.
    cat > "$OKTO_CONFIG" <<EOF
{
  "service": {
    "resp_endpoint": "0.0.0.0:$OKTO_PORT"
  }
}
EOF
}

wait_for_port() {
    local port=$1
    local tries=50
    while [ $tries -gt 0 ]; do
        $CLI -p "$port" PING >/dev/null 2>&1 && return 0
        sleep 0.1
        tries=$((tries - 1))
    done
    return 1
}

kill_listener_on_port() {
    local port=$1
    $CLI -p "$port" SHUTDOWN NOSAVE >/dev/null 2>&1 || true
    local pids
    pids=$(lsof -nP -iTCP:"$port" -sTCP:LISTEN -t 2>/dev/null | sort -u)
    [ -n "$pids" ] && kill $pids 2>/dev/null || true
    sleep 0.3
    pids=$(lsof -nP -iTCP:"$port" -sTCP:LISTEN -t 2>/dev/null | sort -u)
    [ -n "$pids" ] && kill -9 $pids 2>/dev/null || true
    local tries=20
    while [ $tries -gt 0 ]; do
        $CLI -p "$port" PING >/dev/null 2>&1 || return 0
        sleep 0.1
        tries=$((tries - 1))
    done
    return 1
}

start_redis() {
    kill_listener_on_port "$REDIS_PORT"
    $REDIS_SERVER --port "$REDIS_PORT" --daemonize no \
        --save "" --appendonly no \
        > "$LOG_DIR/redis_mem.log" 2>&1 &
    REDIS_PID=$!
    wait_for_port "$REDIS_PORT" || { echo "redis failed to start"; exit 1; }
}

stop_redis() {
    [ -n "${REDIS_PID:-}" ] && {
        $CLI -p "$REDIS_PORT" SHUTDOWN NOSAVE >/dev/null 2>&1 || kill "$REDIS_PID" 2>/dev/null
        wait "$REDIS_PID" 2>/dev/null
    }
    REDIS_PID=
}

start_okto() {
    kill_listener_on_port "$OKTO_PORT"
    ensure_okto_config
    "$OKTO_BIN" -c "$OKTO_CONFIG" > "$LOG_DIR/oktoplus_mem.log" 2>&1 &
    OKTO_PID=$!
    wait_for_port "$OKTO_PORT" || { echo "oktoplus failed to start"; exit 1; }
}

stop_okto() {
    [ -n "${OKTO_PID:-}" ] && { kill "$OKTO_PID" 2>/dev/null; wait "$OKTO_PID" 2>/dev/null; }
    OKTO_PID=
}

cleanup() { stop_redis; stop_okto; }
trap cleanup EXIT INT TERM

# RSS in KiB from /proc/<pid>/status (VmRSS line).
rss_kib() {
    local pid=$1
    awk '/^VmRSS:/ {print $2}' "/proc/$pid/status" 2>/dev/null
}

# Load EXACTLY N distinct keys via RPUSH key:i value, piped through
# redis-cli --pipe. Deterministic, no random collisions, both servers
# receive an identical workload. Uses LISTS because Oktoplus doesn't
# implement SET yet (strings at 0% coverage).
load_keys() {
    local port=$1 n=$2 size=$3
    local value
    value=$(printf 'a%.0s' $(seq 1 "$size"))
    awk -v n="$n" -v v="$value" 'BEGIN {
        for (i = 0; i < n; i++) printf "RPUSH key:%d %s\r\n", i, v
    }' | $CLI -p "$port" --pipe >/dev/null 2>&1 || true
}

flush_keys() {
    # FLUSHALL drops every key. Follow with MEMORY PURGE so we measure
    # post-explicit-purge residual on both servers (apples-to-apples):
    # without it, Redis's residual reflects "FLUSHALL + jemalloc lazy
    # decay over ~10s" while Oktoplus would purge inside FLUSHALL and
    # land lower for asymmetric reasons.
    local port=$1
    $CLI -p "$port" FLUSHALL     >/dev/null 2>&1 || true
    $CLI -p "$port" MEMORY PURGE >/dev/null 2>&1 || true
}

# One trial: start fresh, baseline, load, steady, flush, residual.
# Echoes a CSV row:
#   server,n,value_size,baseline_kib,steady_kib,residual_kib,bytes_per_key
trial() {
    local server=$1 port=$2 pid=$3 n=$4 size=$5

    sleep 0.5
    local baseline; baseline=$(rss_kib "$pid")

    load_keys "$port" "$n" "$size"
    sleep 1
    local steady; steady=$(rss_kib "$pid")

    flush_keys "$port"
    sleep 1
    local residual; residual=$(rss_kib "$pid")

    local growth_kib=$((steady - baseline))
    local bpk
    if [ "$n" -gt 0 ]; then
        bpk=$(awk -v g="$growth_kib" -v n="$n" 'BEGIN { printf "%.1f", g * 1024 / n }')
    else
        bpk="0"
    fi

    printf '%s,%d,%d,%d,%d,%d,%s\n' \
        "$server" "$n" "$size" "$baseline" "$steady" "$residual" "$bpk"
}

OUT_CSV="$RAW_DIR/memory.csv"
OUT_MD="$RESULTS_DIR/memory_results.md"

echo "server,n_keys,value_size_b,baseline_rss_kib,steady_rss_kib,residual_rss_kib,bytes_per_key" > "$OUT_CSV"

log "Memory footprint sweep — key counts: $KEY_COUNTS, value sizes: $VALUE_SIZES"

for n in $KEY_COUNTS; do
    for size in $VALUE_SIZES; do
        log "Trial: n=$n value=${size}B"

        start_redis
        row=$(trial "redis" "$REDIS_PORT" "$REDIS_PID" "$n" "$size")
        echo "$row" >> "$OUT_CSV"
        log "  redis    : $row"
        stop_redis

        start_okto
        row=$(trial "oktoplus" "$OKTO_PORT" "$OKTO_PID" "$n" "$size")
        echo "$row" >> "$OUT_CSV"
        log "  oktoplus : $row"
        stop_okto
    done
done

log "Done. Raw CSV: $OUT_CSV"

# Render a markdown table at $OUT_MD comparing the two side-by-side.
python3 - "$OUT_CSV" > "$OUT_MD" <<'PY'
import csv, sys
from collections import defaultdict

path = sys.argv[1]
rows = list(csv.DictReader(open(path)))

# Group by (n, size) -> server -> row
by_case = defaultdict(dict)
for r in rows:
    by_case[(int(r["n_keys"]), int(r["value_size_b"]))][r["server"]] = r

print("# Memory footprint — Oktoplus vs Redis")
print()
print("Each trial: start fresh server, record baseline RSS, load N "
      "distinct keys via `RPUSH key:i <value>` piped through "
      "`redis-cli --pipe` (deterministic, no random collisions, "
      "identical workload to both servers), record steady-state RSS, "
      "FLUSHALL, record residual RSS.")
print()
print("`bytes/key = (steady - baseline) * 1024 / N`. Lower is better. "
      "`residual` is what the allocator hangs on to after FLUSHALL — "
      "lower is better, but allocators legitimately retain pages for "
      "reuse.")
print()
print("| N keys | value | server | baseline (KiB) | steady (KiB) | residual (KiB) | bytes/key |")
print("|-------:|------:|--------|---------------:|-------------:|---------------:|----------:|")

for (n, size), per in sorted(by_case.items()):
    for server in ("oktoplus", "redis"):
        if server not in per:
            continue
        r = per[server]
        print(f"| {n:>6} | {size:>4}B | {server:<8} | "
              f"{int(r['baseline_rss_kib']):>14} | "
              f"{int(r['steady_rss_kib']):>12} | "
              f"{int(r['residual_rss_kib']):>14} | "
              f"{r['bytes_per_key']:>9} |")

# Per-case okto/redis bytes-per-key ratio summary.
print()
print("## Bytes/key ratio (Oktoplus / Redis)")
print()
print("| N keys | value | okto bpk | redis bpk | okto / redis |")
print("|-------:|------:|---------:|----------:|-------------:|")
for (n, size), per in sorted(by_case.items()):
    if "oktoplus" not in per or "redis" not in per:
        continue
    o = float(per["oktoplus"]["bytes_per_key"])
    r = float(per["redis"]["bytes_per_key"])
    ratio = o / r if r > 0 else float("inf")
    print(f"| {n:>6} | {size:>4}B | {o:>8.1f} | {r:>9.1f} | {ratio:>11.2f} |")
PY

log "Markdown summary: $OUT_MD"
