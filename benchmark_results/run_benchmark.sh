#!/usr/bin/env bash
# Benchmark: Redis (port 6380) vs Oktoplus (port 6379)
# Speed (single-client, pipeline 1 & 16) + Parallelism (1-200 clients, random keys)

BENCH=~/git_store/redis/src/redis-benchmark
CLI=~/git_store/redis/src/redis-cli
RESULTS_DIR=~/git_store/oktoplus/benchmark_results
REDIS_PORT=6380
OKTO_PORT=6379
NUM_OPS=100000
KEYSPACE=100000

mkdir -p "$RESULTS_DIR/raw"

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

run_bench() {
    local port=$1
    local clients=$2
    local pipeline=$3
    shift 3
    $BENCH -h 127.0.0.1 -p "$port" -n "$NUM_OPS" -c "$clients" -P "$pipeline" \
        -r "$KEYSPACE" --csv "$@" 2>/dev/null | extract_data_rows
}

run_builtin_bench() {
    local port=$1
    local clients=$2
    local pipeline=$3
    local tests=$4
    $BENCH -h 127.0.0.1 -p "$port" -n "$NUM_OPS" -c "$clients" -P "$pipeline" \
        -r "$KEYSPACE" -t "$tests" --csv 2>/dev/null | extract_data_rows
}

CSV_HEADER='"test","rps","avg_latency_ms","min_latency_ms","p50_latency_ms","p95_latency_ms","p99_latency_ms","max_latency_ms"'

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

log "All benchmarks complete. Raw results in $RESULTS_DIR/raw/"
