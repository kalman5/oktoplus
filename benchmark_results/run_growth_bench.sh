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

# Defaults reproduce the PUBLISHED README numbers out of the box.
# Override via env vars for variations.
PLATEAUS=${PLATEAUS:-"1000 10000 100000 1000000"}
QUERY_OPS=${QUERY_OPS:-50000}     # ops per query test
ITERATIONS=${ITERATIONS:-3}       # median-of-N for each query test
PIPELINE=${PIPELINE:-16}

mkdir -p "$RAW_DIR" "$LOG_DIR"

log() { echo "[$(date '+%H:%M:%S')] $*"; }

# Effective config -- echoed at startup and stamped next to each CSV
# so a stray output file is self-describing.
CONFIG_LINE="PLATEAUS=\"$PLATEAUS\" QUERY_OPS=$QUERY_OPS ITERATIONS=$ITERATIONS PIPELINE=$PIPELINE"
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

# Read combined utime+stime in clock ticks for a PID.
CLK_TCK=$(getconf CLK_TCK)
cpu_ticks() {
    [ -e "/proc/$1/stat" ] || { echo 0; return; }
    awk '{print $14 + $15}' "/proc/$1/stat" 2>/dev/null || echo 0
}

# Run a single (already-pipelined) command N_ITER times, emit the
# aggregated median row prefixed with the list size + the server's
# average %CPU during the call window. Output goes to stdout in our
# enriched CSV format.
run_op() {
    local list_size=$1; shift
    local op_label=$1; shift
    local cpu_before wall_start agg_csv wall_end cpu_after cpu_pct
    cpu_before=$(cpu_ticks "$SERVER_PID")
    wall_start=$(date +%s.%N)
    agg_csv=$(
        {
            for i in $(seq 1 "$ITERATIONS"); do
                $BENCH -h 127.0.0.1 -p "$SERVER_PORT" -P "$PIPELINE" --csv "$@" \
                    2>/dev/null | extract_data_rows
            done
        } | aggregate 2>/dev/null | tr -d '\r'
    )
    wall_end=$(date +%s.%N)
    cpu_after=$(cpu_ticks "$SERVER_PID")
    cpu_pct=$(awk -v cb="$cpu_before" -v ca="$cpu_after" \
                  -v ws="$wall_start" -v we="$wall_end" -v t="$CLK_TCK" \
                  'BEGIN { dt = we - ws; if (dt > 0 && t > 0) printf "%.0f", (ca - cb) / t / dt * 100; else print 0 }')
    echo "$agg_csv" | awk -F',' -v sz="$list_size" -v lbl="$op_label" -v cpu="$cpu_pct" '
        {
            printf "%d,\"%s\"", sz, lbl
            for (i=2; i<=NF; i++) printf ",%s", $i
            printf ",%s\n", cpu
        }'
}

run_growth_test() {
    local server=$1
    local outfile="$RAW_DIR/growth_${server}.csv"

    log "Growth test: $server"
    start_server "$server"

    echo 'list_size,test,rps,avg_latency_ms,min_latency_ms,p50_latency_ms,p95_latency_ms,p99_latency_ms,max_latency_ms,server_cpu_pct' > "$outfile"
    echo "$CONFIG_LINE" > "${outfile%.csv}.config"

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
