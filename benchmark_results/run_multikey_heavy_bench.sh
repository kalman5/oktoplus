#!/usr/bin/env bash
# Multi-client, different-keys, heavy-per-command bench.
#
# Pre-populates K keys with N elements each, then has C concurrent
# clients hammer LRANGE on random keys at -P 16. Each command does
# real per-element work (memcpy + RESP framing for 100..600 strings)
# so per-command CPU is non-trivial -- this is the workload where
# Oktoplus's per-key mutexes + 32 shards should pull away from
# Redis's single-threaded executor.
#
# Output: raw/multikey_${server}.csv with columns
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
# out of the box; runtimes land in the few-minutes range. Override
# via env vars for variations.
KEY_COUNTS=${KEY_COUNTS:-"10000"}
ELEM_COUNTS=${ELEM_COUNTS:-"100 1000"}
CONCURRENCIES=${CONCURRENCIES:-"1 16 64 128"}
QUERY_OPS=${QUERY_OPS:-100000}
PIPELINE=${PIPELINE:-16}
ITERATIONS=${ITERATIONS:-3}

mkdir -p "$RAW_DIR" "$LOG_DIR"

log() { echo "[$(date '+%H:%M:%S')] $*"; }

# Effective config -- echoed at startup and stamped next to each CSV.
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
            > "$LOG_DIR/redis_multikey.log" 2>&1 &
        SERVER_PID=$!; SERVER_PORT=$REDIS_PORT
        wait_for_port "$REDIS_PORT" || { echo "redis failed"; exit 1; }
        ;;
      oktoplus)
        kill_listener_on_port "$OKTO_PORT"
        ensure_okto_config
        "$OKTO_BIN" -c "$OKTO_CONFIG" > "$LOG_DIR/oktoplus_multikey.log" 2>&1 &
        SERVER_PID=$!; SERVER_PORT=$OKTO_PORT
        wait_for_port "$OKTO_PORT" || { echo "oktoplus failed"; exit 1; }
        ;;
    esac
}

stop_server() {
    [ -n "${SERVER_PID:-}" ] || return 0
    # SIGTERM unconditionally; redis-cli SHUTDOWN returns success even
    # against oktoplus (which doesn't implement it), so falling back
    # via `||` would never run kill and `wait` would block forever.
    kill "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null
    SERVER_PID=
}

trap stop_server EXIT INT TERM

extract_data_rows() { grep -v '^"test"' || true; }
aggregate() { python3 "$RESULTS_DIR/bench_aggregate.py"; }

# Deterministic K x N populate via redis-cli --pipe.
#
# Keys are written zero-padded to 12 digits so they match
# redis-benchmark's `__rand_int__` substitution (which always
# expands to a 12-char zero-padded integer). Earlier benches that
# populated `key:%d` and queried `key:__rand_int__` measured
# nothing but key-not-found responses.
populate() {
    local K=$1 N=$2
    local val
    val=$(printf 'a%.0s' $(seq 1 16))     # 16-byte value
    awk -v K="$K" -v N="$N" -v V="$val" 'BEGIN {
        for (k = 0; k < K; k++)
            for (i = 0; i < N; i++)
                printf "RPUSH key:%012d %s\r\n", k, V
    }' | $CLI -p "$SERVER_PORT" --pipe >/dev/null 2>&1 || true
}

# Run a redis-benchmark command at a given concurrency, ITERATIONS
# times, emit one row prefixed with (K, N, clients, test_label).
run_op() {
    local K=$1 N=$2 C=$3 label=$4; shift 4
    local agg_csv
    agg_csv=$(
        {
            for i in $(seq 1 "$ITERATIONS"); do
                $BENCH -h 127.0.0.1 -p "$SERVER_PORT" -c "$C" -P "$PIPELINE" --csv \
                    -n "$QUERY_OPS" -r "$K" "$@" 2>/dev/null | extract_data_rows
            done
        } | aggregate 2>/dev/null
    )
    echo "$agg_csv" | awk -F',' -v K="$K" -v N="$N" -v C="$C" -v lbl="$label" '
        {
            printf "%d,%d,%d,\"%s\"", K, N, C, lbl
            for (i=2; i<=NF; i++) printf ",%s", $i
            printf "\n"
        }'
}

run_multikey_test() {
    local server=$1
    local outfile="$RAW_DIR/multikey_${server}.csv"

    log "Multi-key heavy test: $server"
    start_server "$server"

    echo 'K_keys,N_elements,clients,test,rps,avg_latency_ms,min_latency_ms,p50_latency_ms,p95_latency_ms,p99_latency_ms,max_latency_ms' > "$outfile"
    echo "$CONFIG_LINE" > "${outfile%.csv}.config"

    for K in $KEY_COUNTS; do
        for N in $ELEM_COUNTS; do
            log "  populate K=$K N=$N"
            $CLI -p "$SERVER_PORT" FLUSHALL >/dev/null 2>&1
            populate "$K" "$N"

            for C in $CONCURRENCIES; do
                log "    LRANGE workloads at c=$C"
                # LRANGE 0 99: short slice; bytes per response = 100*16 = 1.6KB.
                run_op "$K" "$N" "$C" "LRANGE_0_99" \
                    LRANGE "key:__rand_int__" 0 99 >> "$outfile"
                # LRANGE 0 -1: full list (N elements, varies the wire size).
                run_op "$K" "$N" "$C" "LRANGE_0_-1" \
                    LRANGE "key:__rand_int__" 0 -1 >> "$outfile"
                # LINDEX at index 0 -- per-call work is tiny but the
                # test spreads contention across keys (random key,
                # fixed index because redis-benchmark's __rand_int__
                # zero-pads to 12 chars and LINDEX's integer parser
                # rejects leading zeros).
                run_op "$K" "$N" "$C" "LINDEX_0" \
                    LINDEX "key:__rand_int__" 0 >> "$outfile"
            done
        done
    done

    log "  -> saved to $outfile"
    stop_server
}

run_multikey_test "redis"
run_multikey_test "oktoplus"

log "multikey heavy bench complete."
