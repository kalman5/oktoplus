#!/bin/bash
# Sets memory benchmark: bytes-per-MEMBER for SADD workloads, Oktoplus
# vs Redis. The published run_memory.sh measures LISTS (RPUSH) and so
# cannot show the Sets storage change (std::string -> okts::stor::string,
# 32 B -> 16 B per member). This script loads sets with many members per
# key, where per-member slot cost dominates over the per-key overhead.
#
#   bytes/member = (steady_rss - baseline_rss) * 1024 / total_members
#
# Regimes (each ~1M members total so the signal is well above noise):
#   int-big    : 10 sets x 100000 integer members  (both servers hashtable; >512 intset cap)
#   int-512    : ~1953 sets x 512 integer members   (Redis uses intset 8B; Okto flat_hash_set)
#   str12      : 10 sets x 100000 12-byte members    (<=15 B -> Okto pure SSO, no heap)
set -u

CLI=~/git_store/redis/src/redis-cli
REDIS_SERVER=~/git_store/redis/src/redis-server
OKTO_BIN=~/git_store/oktoplus/build/optimized/src/Executables/Oktoplus/oktoplus
OKTO_CONFIG=/tmp/oktoplus_bench_sets.json
RESULTS_DIR=~/git_store/oktoplus/benchmark_results
LOG_DIR=$RESULTS_DIR/server_logs
RAW_DIR=$RESULTS_DIR/raw
REDIS_PORT=6380
OKTO_PORT=6379

mkdir -p "$RAW_DIR" "$LOG_DIR"
log() { echo "[$(date '+%H:%M:%S')] $*"; }

ensure_okto_config() {
    cat > "$OKTO_CONFIG" <<EOF
{ "service": { "resp_endpoint": "0.0.0.0:$OKTO_PORT" } }
EOF
}

wait_for_port() {
    local port=$1 tries=50
    while [ $tries -gt 0 ]; do
        $CLI -p "$port" PING >/dev/null 2>&1 && return 0
        sleep 0.1; tries=$((tries - 1))
    done
    return 1
}

kill_listener_on_port() {
    local port=$1 pids
    $CLI -p "$port" SHUTDOWN NOSAVE >/dev/null 2>&1 || true
    pids=$(lsof -nP -iTCP:"$port" -sTCP:LISTEN -t 2>/dev/null | sort -u)
    [ -n "$pids" ] && kill $pids 2>/dev/null || true
    sleep 0.3
    pids=$(lsof -nP -iTCP:"$port" -sTCP:LISTEN -t 2>/dev/null | sort -u)
    [ -n "$pids" ] && kill -9 $pids 2>/dev/null || true
}

start_redis() {
    kill_listener_on_port "$REDIS_PORT"
    $REDIS_SERVER --port "$REDIS_PORT" --daemonize no --save "" --appendonly no \
        > "$LOG_DIR/redis_sets.log" 2>&1 &
    REDIS_PID=$!
    wait_for_port "$REDIS_PORT" || { echo "redis failed to start"; exit 1; }
}
stop_redis() {
    [ -n "${REDIS_PID:-}" ] && { $CLI -p "$REDIS_PORT" SHUTDOWN NOSAVE >/dev/null 2>&1 || kill "$REDIS_PID" 2>/dev/null; wait "$REDIS_PID" 2>/dev/null; }
    REDIS_PID=
}
start_okto() {
    kill_listener_on_port "$OKTO_PORT"
    ensure_okto_config
    MALLOC_CONF="narenas:1,dirty_decay_ms:0,muzzy_decay_ms:0,tcache:false,background_thread:true" "$OKTO_BIN" -c "$OKTO_CONFIG" > "$LOG_DIR/oktoplus_sets.log" 2>&1 &
    OKTO_PID=$!
    wait_for_port "$OKTO_PORT" || { echo "oktoplus failed to start"; exit 1; }
}
stop_okto() {
    [ -n "${OKTO_PID:-}" ] && { kill "$OKTO_PID" 2>/dev/null; wait "$OKTO_PID" 2>/dev/null; }
    OKTO_PID=
}
cleanup() { stop_redis; stop_okto; }
trap cleanup EXIT INT TERM

rss_kib() { awk '/^VmRSS:/ {print $2}' "/proc/$1/status" 2>/dev/null; }

# Emit SADD commands for the named regime to stdout (RESP inline, for --pipe).
gen_load() {
    local regime=$1
    case "$regime" in
        int-big) awk 'BEGIN { for (k=0;k<10;k++) for (m=0;m<100000;m++) printf "SADD s:%d %d\r\n", k, m }' ;;
        int-512) awk 'BEGIN { for (k=0;k<1953;k++) for (m=0;m<512;m++) printf "SADD s:%d %d\r\n", k, m }' ;;
        str12)   awk 'BEGIN { for (k=0;k<10;k++) for (m=0;m<100000;m++) printf "SADD s:%d m%011d\r\n", k, m }' ;;
    esac
}
members_of() {
    case "$1" in
        int-big) echo 1000000 ;;
        int-512) echo $((1953*512)) ;;
        str12)   echo 1000000 ;;
    esac
}

trial() {
    local server=$1 port=$2 pid=$3 regime=$4
    local members; members=$(members_of "$regime")
    sleep 0.5
    local baseline; baseline=$(rss_kib "$pid")
    gen_load "$regime" | $CLI -p "$port" --pipe >/dev/null 2>&1 || true
    sleep 1
    local steady; steady=$(rss_kib "$pid")
    $CLI -p "$port" FLUSHALL >/dev/null 2>&1 || true
    $CLI -p "$port" MEMORY PURGE >/dev/null 2>&1 || true
    local growth=$((steady - baseline))
    local bpm; bpm=$(awk -v g="$growth" -v n="$members" 'BEGIN { printf "%.1f", g*1024/n }')
    printf '%s,%s,%d,%d,%d,%d,%s\n' "$server" "$regime" "$members" "$baseline" "$steady" "$growth" "$bpm"
}

OUT_CSV="$RAW_DIR/memory_sets.csv"
echo "server,regime,members,baseline_kib,steady_kib,growth_kib,bytes_per_member" > "$OUT_CSV"

REGIMES=${REGIMES:-"int-big int-512 str12"}

for regime in $REGIMES; do
    log "Regime: $regime"
    start_redis;  trial redis    "$REDIS_PORT" "$REDIS_PID" "$regime" | tee -a "$OUT_CSV"; stop_redis
    start_okto;   trial oktoplus "$OKTO_PORT"  "$OKTO_PID"  "$regime" | tee -a "$OUT_CSV"; stop_okto
done

echo
echo "==== Sets memory: bytes/member (lower is better) ===="
awk -F, 'NR>1 { v[$2","$1]=$7; seen[$2]=1 }
END {
  printf "%-10s %14s %14s %14s\n", "regime", "okto b/mem", "redis b/mem", "okto/redis"
  for (r in seen) {
    o=v[r",oktoplus"]; rd=v[r",redis"];
    ratio=(rd>0)? sprintf("%.2fx", o/rd) : "n/a";
    printf "%-10s %14s %14s %14s\n", r, o, rd, ratio
  }
}' "$OUT_CSV" | sort
echo
echo "Raw CSV: $OUT_CSV"
