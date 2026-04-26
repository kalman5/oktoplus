#!/usr/bin/env bash
# Single-key list growth bench: how does push throughput, full-range
# read, partial-range read, and random-index access scale as one list
# grows from 1k -> 1M elements?
#
# Exposes the gap that the random-key benches don't: every list there
# stays at <=1 element, so no container's growth behaviour shows up.
# This bench drives one fixed key to plateau sizes and times each
# operation at each size.
#
# Output:  raw/growth_${server}.csv with columns
#   list_size,test,rps,avg_latency_ms,min_latency_ms,p50_latency_ms,p95_latency_ms,p99_latency_ms,max_latency_ms

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

PLATEAUS=${PLATEAUS:-"1000 10000 100000 1000000"}
QUERY_OPS=${QUERY_OPS:-50000}     # ops per query test
ITERATIONS=${ITERATIONS:-3}       # median-of-N for each query test
PIPELINE=${PIPELINE:-16}

mkdir -p "$RAW_DIR" "$LOG_DIR"

log() { echo "[$(date '+%H:%M:%S')] $*"; }

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
            > "$LOG_DIR/redis_growth.log" 2>&1 &
        SERVER_PID=$!; SERVER_PORT=$REDIS_PORT
        wait_for_port "$REDIS_PORT" || { echo "redis failed"; exit 1; }
        ;;
      oktoplus)
        kill_listener_on_port "$OKTO_PORT"
        ensure_okto_config
        "$OKTO_BIN" -c "$OKTO_CONFIG" > "$LOG_DIR/oktoplus_growth.log" 2>&1 &
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

# Run a single (already-pipelined) command N_ITER times, emit the
# aggregated median row prefixed with the list size. Output goes to
# stdout in our enriched CSV format.
run_op() {
    local list_size=$1; shift
    local op_label=$1; shift
    # Remaining args are passed straight to redis-benchmark.
    local agg_csv
    agg_csv=$(
        {
            for i in $(seq 1 "$ITERATIONS"); do
                $BENCH -h 127.0.0.1 -p "$SERVER_PORT" -P "$PIPELINE" --csv "$@" \
                    2>/dev/null | extract_data_rows
            done
        } | aggregate 2>/dev/null
    )
    # agg_csv is one row: "test","rps","avg",...
    # Prepend list_size and replace the test name with our explicit label.
    echo "$agg_csv" | awk -F',' -v sz="$list_size" -v lbl="$op_label" '
        {
            # rebuild row: list_size, label, rest of columns from index 2
            printf "%d,\"%s\"", sz, lbl
            for (i=2; i<=NF; i++) printf ",%s", $i
            printf "\n"
        }'
}

run_growth_test() {
    local server=$1
    local outfile="$RAW_DIR/growth_${server}.csv"

    log "Growth test: $server"
    start_server "$server"

    echo 'list_size,test,rps,avg_latency_ms,min_latency_ms,p50_latency_ms,p95_latency_ms,p99_latency_ms,max_latency_ms' > "$outfile"

    for plateau in $PLATEAUS; do
        log "  plateau N=$plateau"
        $CLI -p "$SERVER_PORT" FLUSHALL >/dev/null 2>&1

        # 1. Push to grow "mylist" to $plateau elements. Skip tiny
        #    plateaus where rps measurement is dominated by startup
        #    cost (-n < ~5000 returns garbage).
        if [ "$plateau" -ge 5000 ]; then
            run_op "$plateau" "PUSH_to_N" -t lpush -n "$plateau" >> "$outfile"
        else
            $CLI -p "$SERVER_PORT" RPUSH mylist $(seq 1 "$plateau") >/dev/null
        fi

        # 2. LRANGE small slice (always 0..99). Skip when the whole
        #    list is smaller than 100 (returns the full list which is
        #    measured in step 3).
        if [ "$plateau" -gt 100 ]; then
            run_op "$plateau" "LRANGE_0_99" -n "$QUERY_OPS" \
                LRANGE mylist 0 99 >> "$outfile"
        fi

        # 3. LRANGE up to first 1000 elements (or whole list if smaller).
        #    This is the real "iterator + region walk" stress test.
        local stop=$((plateau - 1))
        if [ "$plateau" -gt 1000 ]; then stop=999; fi
        run_op "$plateau" "LRANGE_0_${stop}" -n "$QUERY_OPS" \
            LRANGE mylist 0 "$stop" >> "$outfile"

        # 4. LINDEX at fixed positions: front, midpoint, back. Three
        #    cheap O(1) probes that show whether the 3-region branch
        #    in SplitList costs anything per call.
        local mid=$((plateau / 2))
        local last=$((plateau - 1))
        run_op "$plateau" "LINDEX_0"     -n "$QUERY_OPS" LINDEX mylist 0      >> "$outfile"
        run_op "$plateau" "LINDEX_mid"   -n "$QUERY_OPS" LINDEX mylist "$mid" >> "$outfile"
        run_op "$plateau" "LINDEX_last"  -n "$QUERY_OPS" LINDEX mylist "$last" >> "$outfile"
    done

    log "  -> saved to $outfile"
    stop_server
}

run_growth_test "redis"
run_growth_test "oktoplus"

log "growth bench complete."
