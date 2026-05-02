#!/usr/bin/env bash
# Parallelism-advantage bench: workloads where command-execution CPU
# dominates wire bytes, so a single-threaded executor (Redis) stays
# capped at one core while a sharded multi-threaded server (Oktoplus)
# scales with -c.
#
# Workload: LPOS key:__rand_int__ does_not_exist  -- searches every
# element of the target list and returns nil (~5 bytes of response).
# Pre-populated lists have N elements of distinct strings, so each
# call walks the whole list. CPU per call ~= O(N) string compares,
# response wire bytes ~= O(1).
#
# Output: raw/parallelism_${server}.csv
#   K_keys,N_elements,clients,test,rps,avg_latency_ms,min_latency_ms,p50_latency_ms,p95_latency_ms,p99_latency_ms,max_latency_ms

set -u

BENCH=~/git_store/redis/src/redis-benchmark
CLI=~/git_store/redis/src/redis-cli
REDIS_SERVER=~/git_store/redis/src/redis-server
OKTO_BIN=~/git_store/oktoplus/build/optimized/src/Executables/Oktoplus/oktoplus
OKTO_CONFIG=/tmp/oktoplus_bench.json
RESULTS_DIR=~/git_store/oktoplus/benchmark_results
LOG_DIR=$RESULTS_DIR/server_logs
RAW_DIR=$RESULTS_DIR/raw
REDIS_PORT=6380
OKTO_PORT=6379

# Sweep parameters. Defaults reproduce the PUBLISHED README numbers
# out of the box -- if you change them, you are running a different
# workload than what's plotted in the parallelism chart. Override via
# env vars for variations (e.g. `PIPELINE=1 bash ...` to measure pure
# concurrency with no batching).
KEY_COUNTS=${KEY_COUNTS:-"1000"}
ELEM_COUNTS=${ELEM_COUNTS:-"1000 10000"}
CONCURRENCIES=${CONCURRENCIES:-"1 4 16 64 128"}
QUERY_OPS=${QUERY_OPS:-50000}
PIPELINE=${PIPELINE:-16}           # 16 batches per round-trip; matches README
ITERATIONS=${ITERATIONS:-3}

mkdir -p "$RAW_DIR" "$LOG_DIR"

log() { echo "[$(date '+%H:%M:%S')] $*"; }

# Echo the effective config up front so anyone reading the log sees
# what was actually measured. Same string also gets stamped into the
# CSV header so a stray output file is self-describing.
CONFIG_LINE="KEY_COUNTS=\"$KEY_COUNTS\" ELEM_COUNTS=\"$ELEM_COUNTS\" CONCURRENCIES=\"$CONCURRENCIES\" QUERY_OPS=$QUERY_OPS PIPELINE=$PIPELINE ITERATIONS=$ITERATIONS"
log "Config: $CONFIG_LINE"

ensure_okto_config() {
    if [ ! -f "$OKTO_CONFIG" ]; then
        cat > "$OKTO_CONFIG" <<EOF
{
  "service": {
    "endpoint": "0.0.0.0:50051",
    "numcqs": 4,
    "minpollers": 4,
    "maxpollers": 16,
    "resp_endpoint": "0.0.0.0:$OKTO_PORT"
  }
}
EOF
    fi
}

wait_for_port() {
    local port=$1; local tries=50
    while [ $tries -gt 0 ]; do
        $CLI -p "$port" PING >/dev/null 2>&1 && return 0
        sleep 0.1; tries=$((tries-1))
    done; return 1
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
        sleep 0.1; tries=$((tries-1))
    done; return 1
}

start_server() {
    local kind=$1
    case $kind in
      redis)
        kill_listener_on_port "$REDIS_PORT"
        $REDIS_SERVER --port "$REDIS_PORT" --daemonize no --save "" --appendonly no \
            > "$LOG_DIR/redis_parallel.log" 2>&1 &
        SERVER_PID=$!; SERVER_PORT=$REDIS_PORT
        wait_for_port "$REDIS_PORT" || { echo "redis failed"; exit 1; }
        ;;
      oktoplus)
        kill_listener_on_port "$OKTO_PORT"
        ensure_okto_config
        "$OKTO_BIN" -c "$OKTO_CONFIG" > "$LOG_DIR/oktoplus_parallel.log" 2>&1 &
        SERVER_PID=$!; SERVER_PORT=$OKTO_PORT
        wait_for_port "$OKTO_PORT" || { echo "oktoplus failed"; exit 1; }
        ;;
    esac
}

stop_server() {
    [ -n "${SERVER_PID:-}" ] || return 0
    kill "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null
    SERVER_PID=
}

trap stop_server EXIT INT TERM

extract_data_rows() { grep -v '^"test"' || true; }
aggregate() { python3 "$RESULTS_DIR/bench_aggregate.py"; }

# Deterministic K x N populate. Each key holds a list of N distinct
# values (val_<k>_<i>) so LPOS for a globally-unknown sentinel always
# scans the whole list.
#
# Keys are written zero-padded to 12 digits so they match
# redis-benchmark's `__rand_int__` substitution.
populate() {
    local K=$1 N=$2
    awk -v K="$K" -v N="$N" 'BEGIN {
        for (k = 0; k < K; k++)
            for (i = 0; i < N; i++)
                printf "RPUSH key:%012d val_%d_%d\r\n", k, k, i
    }' | $CLI -p "$SERVER_PORT" --pipe >/dev/null 2>&1 || true
}

# Read combined utime+stime in clock ticks for a PID. Returns 0 if
# the PID is gone, so subsequent arithmetic doesn't NaN.
CLK_TCK=$(getconf CLK_TCK)
cpu_ticks() {
    [ -e "/proc/$1/stat" ] || { echo 0; return; }
    awk '{print $14 + $15}' "/proc/$1/stat" 2>/dev/null || echo 0
}

# Run a redis-benchmark command at a given concurrency, ITERATIONS
# times, emit one row prefixed with (K, N, clients, test_label).
# Also samples the server's CPU usage across the whole call window
# and appends it as the trailing `server_cpu_pct` column.
#
# server_cpu_pct >100% means multi-core utilisation (e.g. 800% =
# eight cores fully saturated). Redis is single-threaded so its
# value caps near 100%; Oktoplus's sharded design can scale up.
run_op() {
    local K=$1 N=$2 C=$3 label=$4; shift 4
    local cpu_before wall_start agg_csv wall_end cpu_after cpu_pct
    cpu_before=$(cpu_ticks "$SERVER_PID")
    wall_start=$(date +%s.%N)
    agg_csv=$(
        {
            for i in $(seq 1 "$ITERATIONS"); do
                $BENCH -h 127.0.0.1 -p "$SERVER_PORT" -c "$C" -P "$PIPELINE" --csv \
                    -n "$QUERY_OPS" -r "$K" "$@" 2>/dev/null | extract_data_rows
            done
        } | aggregate 2>/dev/null | tr -d '\r'
    )
    wall_end=$(date +%s.%N)
    cpu_after=$(cpu_ticks "$SERVER_PID")
    cpu_pct=$(awk -v cb="$cpu_before" -v ca="$cpu_after" \
                  -v ws="$wall_start" -v we="$wall_end" -v t="$CLK_TCK" \
                  'BEGIN { dt = we - ws; if (dt > 0 && t > 0) printf "%.0f", (ca - cb) / t / dt * 100; else print 0 }')
    echo "$agg_csv" | awk -F',' -v K="$K" -v N="$N" -v C="$C" -v lbl="$label" -v cpu="$cpu_pct" '
        {
            printf "%d,%d,%d,\"%s\"", K, N, C, lbl
            for (i=2; i<=NF; i++) printf ",%s", $i
            printf ",%s\n", cpu
        }'
}

run_parallelism_test() {
    local server=$1
    local outfile="$RAW_DIR/parallelism_${server}.csv"

    log "Parallelism-advantage test: $server"
    start_server "$server"

    echo 'K_keys,N_elements,clients,test,rps,avg_latency_ms,min_latency_ms,p50_latency_ms,p95_latency_ms,p99_latency_ms,max_latency_ms,server_cpu_pct' > "$outfile"
    # Sidecar config stamp -- a stray .csv is always paired with a
    # .config file describing the workload it came from. Keeps the
    # CSV itself parseable by csv.DictReader (no leading comment
    # lines).
    echo "$CONFIG_LINE" > "${outfile%.csv}.config"

    for K in $KEY_COUNTS; do
        for N in $ELEM_COUNTS; do
            log "  populate K=$K N=$N"
            $CLI -p "$SERVER_PORT" FLUSHALL >/dev/null 2>&1
            populate "$K" "$N"

            for C in $CONCURRENCIES; do
                log "    LPOS scan (full-list, sentinel never present) at c=$C"
                # LPOS with a missing value scans every element. Returns
                # (nil), 5 bytes back. Pure CPU per call.
                run_op "$K" "$N" "$C" "LPOS_miss" \
                    LPOS "key:__rand_int__" "__OKTO_SENTINEL_NOT_PRESENT__" >> "$outfile"
                # LREM count=0 of a missing value: also scans whole list,
                # returns integer 0 (~3 bytes). Same CPU profile, different
                # codepath.
                run_op "$K" "$N" "$C" "LREM_miss" \
                    LREM "key:__rand_int__" 0 "__OKTO_SENTINEL_NOT_PRESENT__" >> "$outfile"
            done
        done
    done

    log "  -> saved to $outfile"
    stop_server
}

run_parallelism_test "redis"
run_parallelism_test "oktoplus"

log "parallelism-advantage bench complete."
