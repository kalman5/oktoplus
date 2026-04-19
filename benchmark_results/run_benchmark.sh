#!/usr/bin/env bash
# Benchmark: Redis (port 6380) vs Oktoplus (port 6379)
# Speed (single-client, pipeline 1 & 16) + Parallelism (1-200 clients, random keys)

BENCH=~/git_store/redis/src/redis-benchmark
CLI=~/git_store/redis/src/redis-cli
REDIS_SERVER=~/git_store/redis/src/redis-server
OKTO_BIN=~/git_store/oktoplus/build/optimized/src/Executables/Oktoplus/oktoplus
OKTO_CONFIG=/tmp/oktoplus_bench.json
RESULTS_DIR=~/git_store/oktoplus/benchmark_results
LOG_DIR=$RESULTS_DIR/server_logs
REDIS_PORT=6380
OKTO_PORT=6379
NUM_OPS=100000
KEYSPACE=100000
LARGE_VALUE_SIZE=256
# How many times to repeat each redis-benchmark invocation. The
# aggregator emits the median row across iterations and warns on
# wide spread. Default 1 keeps single-run behaviour for fast A/B;
# set to 5+ for publication-quality numbers.
ITERATIONS=${ITERATIONS:-1}
STARTED_REDIS=0
STARTED_OKTO=0
REDIS_PID=
OKTO_PID=

mkdir -p "$RESULTS_DIR/raw" "$LOG_DIR"

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
    local port=$1
    local tries=50
    while [ $tries -gt 0 ]; do
        if $CLI -p "$port" PING >/dev/null 2>&1; then
            return 0
        fi
        sleep 0.1
        tries=$((tries - 1))
    done
    return 1
}

kill_listener_on_port() {
    local port=$1
    local label=$2
    # Try graceful Redis SHUTDOWN first (covers redis-server and any RESP impl
    # that supports it); fall back to SIGTERM/SIGKILL by port owner.
    $CLI -p "$port" SHUTDOWN NOSAVE >/dev/null 2>&1 || true

    local pids
    pids=$(ss -lptn "sport = :$port" 2>/dev/null \
           | awk 'NR>1 {print $0}' \
           | grep -oE 'pid=[0-9]+' | cut -d= -f2 | sort -u)
    if [ -z "$pids" ]; then
        pids=$(lsof -nP -iTCP:"$port" -sTCP:LISTEN -t 2>/dev/null | sort -u)
    fi

    if [ -n "$pids" ]; then
        log "Killing pre-existing $label on port $port (pids: $(echo $pids | tr '\n' ' '))"
        kill $pids 2>/dev/null || true
        sleep 0.5
        # Anything still listening?
        local still
        still=$(lsof -nP -iTCP:"$port" -sTCP:LISTEN -t 2>/dev/null)
        [ -n "$still" ] && kill -9 $still 2>/dev/null || true
    fi

    # Wait until port is actually free.
    local tries=30
    while [ $tries -gt 0 ]; do
        $CLI -p "$port" PING >/dev/null 2>&1 || return 0
        sleep 0.1
        tries=$((tries - 1))
    done
    echo "ERROR: failed to free port $port for $label" >&2
    return 1
}

start_servers() {
    kill_listener_on_port "$REDIS_PORT" "redis"
    log "Starting Redis on $REDIS_PORT"
    $REDIS_SERVER --port "$REDIS_PORT" --daemonize no \
        --save "" --appendonly no \
        > "$LOG_DIR/redis.log" 2>&1 &
    REDIS_PID=$!
    STARTED_REDIS=1
    if ! wait_for_port "$REDIS_PORT"; then
        echo "ERROR: redis failed to start on $REDIS_PORT" >&2
        cat "$LOG_DIR/redis.log" >&2
        exit 1
    fi

    kill_listener_on_port "$OKTO_PORT" "oktoplus"
    log "Starting Oktoplus on $OKTO_PORT (binary: $OKTO_BIN)${OKTO_LD_PRELOAD:+ [LD_PRELOAD=$OKTO_LD_PRELOAD]}"
    ensure_okto_config
    if [ -n "${OKTO_LD_PRELOAD:-}" ]; then
        LD_PRELOAD="$OKTO_LD_PRELOAD" \
            "$OKTO_BIN" -c "$OKTO_CONFIG" > "$LOG_DIR/oktoplus.log" 2>&1 &
    else
        "$OKTO_BIN" -c "$OKTO_CONFIG" > "$LOG_DIR/oktoplus.log" 2>&1 &
    fi
    OKTO_PID=$!
    STARTED_OKTO=1
    if ! wait_for_port "$OKTO_PORT"; then
        echo "ERROR: oktoplus failed to start on $OKTO_PORT" >&2
        cat "$LOG_DIR/oktoplus.log" >&2
        exit 1
    fi
}

stop_servers() {
    if [ "$STARTED_REDIS" = "1" ] && [ -n "$REDIS_PID" ]; then
        log "Stopping Redis (pid $REDIS_PID)"
        $CLI -p "$REDIS_PORT" SHUTDOWN NOSAVE >/dev/null 2>&1 || kill "$REDIS_PID" 2>/dev/null
        wait "$REDIS_PID" 2>/dev/null
    fi
    if [ "$STARTED_OKTO" = "1" ] && [ -n "$OKTO_PID" ]; then
        log "Stopping Oktoplus (pid $OKTO_PID)"
        kill "$OKTO_PID" 2>/dev/null
        wait "$OKTO_PID" 2>/dev/null
    fi
}

trap stop_servers EXIT INT TERM

flush_server() {
    local port=$1
    $CLI -p "$port" FLUSHALL 2>/dev/null || true
    $CLI -p "$port" FLUSHDB 2>/dev/null || true
}

log() {
    echo "[$(date '+%H:%M:%S')] $*"
}

seed_list_data() {
    local port=$1
    local clients=$2
    local pipeline=${3:-1}
    $BENCH -h 127.0.0.1 -p "$port" -n "$NUM_OPS" -c "$clients" -P "$pipeline" \
        -r "$KEYSPACE" -t lpush 2>/dev/null > /dev/null || true
}

extract_data_rows() {
    grep -v '^"test"' || true
}

aggregate() {
    python3 "$RESULTS_DIR/bench_aggregate.py"
}

run_bench() {
    local port=$1
    local clients=$2
    local pipeline=$3
    shift 3
    {
        for i in $(seq 1 "$ITERATIONS"); do
            $BENCH -h 127.0.0.1 -p "$port" -n "$NUM_OPS" -c "$clients" -P "$pipeline" \
                -r "$KEYSPACE" --csv "$@" 2>/dev/null | extract_data_rows
        done
    } | aggregate
}

run_builtin_bench() {
    local port=$1
    local clients=$2
    local pipeline=$3
    local tests=$4
    {
        for i in $(seq 1 "$ITERATIONS"); do
            $BENCH -h 127.0.0.1 -p "$port" -n "$NUM_OPS" -c "$clients" -P "$pipeline" \
                -r "$KEYSPACE" -t "$tests" --csv 2>/dev/null | extract_data_rows
        done
    } | aggregate
}

# Same as run_builtin_bench but with -d <size>, which tells redis-benchmark
# to pad the built-in test value to <size> bytes.
run_builtin_bench_d() {
    local port=$1
    local clients=$2
    local pipeline=$3
    local tests=$4
    local datasize=$5
    {
        for i in $(seq 1 "$ITERATIONS"); do
            $BENCH -h 127.0.0.1 -p "$port" -n "$NUM_OPS" -c "$clients" -P "$pipeline" \
                -r "$KEYSPACE" -d "$datasize" -t "$tests" --csv 2>/dev/null | extract_data_rows
        done
    } | aggregate
}

CSV_HEADER='"test","rps","avg_latency_ms","min_latency_ms","p50_latency_ms","p95_latency_ms","p99_latency_ms","max_latency_ms"'

# --- Bring up servers ---

start_servers

# --- PART 1: SPEED TESTS (single client, pipeline 1 and 16) ---

run_speed_test() {
    local server_name=$1
    local port=$2
    local pipeline=$3
    local outfile="$RESULTS_DIR/raw/speed_${server_name}_p${pipeline}.csv"

    log "Speed test: $server_name pipeline=$pipeline"

    echo "$CSV_HEADER" > "$outfile"

    flush_server "$port"

    # Built-in: LPUSH, SADD, LRANGE_100
    run_builtin_bench "$port" 1 "$pipeline" "lpush,sadd,lrange_100" >> "$outfile"

    # Seed list data for POP tests
    seed_list_data "$port" 1 "$pipeline"

    # Custom commands
    run_bench "$port" 1 "$pipeline" RPUSH mylist__rand_int__ val >> "$outfile"
    run_bench "$port" 1 "$pipeline" LPOP mylist__rand_int__ >> "$outfile"
    run_bench "$port" 1 "$pipeline" RPOP mylist__rand_int__ >> "$outfile"
    run_bench "$port" 1 "$pipeline" LLEN mylist__rand_int__ >> "$outfile"
    run_bench "$port" 1 "$pipeline" SCARD myset__rand_int__ >> "$outfile"

    log "  -> saved to $outfile"
}

for pipeline in 1 16; do
    run_speed_test "redis" $REDIS_PORT "$pipeline"
    run_speed_test "oktoplus" $OKTO_PORT "$pipeline"
done

# --- PART 2: PARALLELISM TESTS (varying clients, random keys) ---

run_parallel_test() {
    local server_name=$1
    local port=$2
    local clients=$3
    local outfile="$RESULTS_DIR/raw/parallel_${server_name}_c${clients}.csv"

    log "Parallelism test: $server_name clients=$clients"

    echo "$CSV_HEADER" > "$outfile"

    flush_server "$port"

    # Built-in: LPUSH, SADD, LRANGE_100
    run_builtin_bench "$port" "$clients" 1 "lpush,sadd,lrange_100" >> "$outfile"

    # Seed list data for POP tests
    seed_list_data "$port" "$clients"

    # Custom commands (all use random keys for parallelism)
    run_bench "$port" "$clients" 1 RPUSH mylist__rand_int__ val >> "$outfile"
    run_bench "$port" "$clients" 1 LPOP mylist__rand_int__ >> "$outfile"
    run_bench "$port" "$clients" 1 RPOP mylist__rand_int__ >> "$outfile"
    run_bench "$port" "$clients" 1 LLEN mylist__rand_int__ >> "$outfile"
    run_bench "$port" "$clients" 1 SCARD myset__rand_int__ >> "$outfile"

    log "  -> saved to $outfile"
}

CONCURRENCIES=(1 10 50 100 200)

for c in "${CONCURRENCIES[@]}"; do
    run_parallel_test "redis" $REDIS_PORT "$c"
    run_parallel_test "oktoplus" $OKTO_PORT "$c"
done

# --- PART 3: SPEED TESTS WITH LARGE VALUES ---
#
# Same shape as PART 1 but the value is LARGE_VALUE_SIZE bytes
# (default 256). This stresses memcpy, string allocation, and the
# write path; with std::string SSO the previous run hid most of those
# costs because "val" fit inline. Output goes to
# raw/speed_*_p${P}_d${LARGE_VALUE_SIZE}.csv so the existing reports
# don't get mixed up.

run_speed_test_large() {
    local server_name=$1
    local port=$2
    local pipeline=$3
    local outfile="$RESULTS_DIR/raw/speed_${server_name}_p${pipeline}_d${LARGE_VALUE_SIZE}.csv"

    log "Speed test (large $LARGE_VALUE_SIZE-byte values): $server_name pipeline=$pipeline"
    echo "$CSV_HEADER" > "$outfile"

    flush_server "$port"

    # Built-in: -d makes the value LARGE_VALUE_SIZE bytes.
    run_builtin_bench_d "$port" 1 "$pipeline" "lpush,sadd,lrange_100" "$LARGE_VALUE_SIZE" \
        >> "$outfile"

    # Seed list data for POP tests (also with large values).
    $BENCH -h 127.0.0.1 -p "$port" -n "$NUM_OPS" -c 1 -P "$pipeline" \
        -r "$KEYSPACE" -d "$LARGE_VALUE_SIZE" -t lpush 2>/dev/null > /dev/null || true

    # Custom commands with a literal LARGE_VALUE_SIZE-byte value.
    local large_value
    large_value=$(printf 'a%.0s' $(seq 1 "$LARGE_VALUE_SIZE"))

    run_bench "$port" 1 "$pipeline" RPUSH mylist__rand_int__ "$large_value" >> "$outfile"
    run_bench "$port" 1 "$pipeline" LPOP mylist__rand_int__ >> "$outfile"
    run_bench "$port" 1 "$pipeline" RPOP mylist__rand_int__ >> "$outfile"
    run_bench "$port" 1 "$pipeline" LLEN mylist__rand_int__ >> "$outfile"
    run_bench "$port" 1 "$pipeline" SCARD myset__rand_int__ >> "$outfile"

    log "  -> saved to $outfile"
}

for pipeline in 1 16; do
    run_speed_test_large "redis" $REDIS_PORT "$pipeline"
    run_speed_test_large "oktoplus" $OKTO_PORT "$pipeline"
done

log "All benchmarks complete. Raw results in $RESULTS_DIR/raw/"
