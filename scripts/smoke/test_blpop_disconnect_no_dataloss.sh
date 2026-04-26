#!/usr/bin/env bash
# Regression smoke test: a client that disconnects mid-BLPOP must
# not cause subsequent pushes to silently lose their data. Before
# the fix (commit hash to be filled on commit) the storage drain
# would transfer the value out of the source list to the dead
# waiter's onWake callback, the callback's weak_ptr to the
# Connection would fail to lock, and the value was discarded.
#
# Reproduce:
#   1. Start oktoplus.
#   2. Open a redis-cli BLPOP for a fresh key.
#   3. Kill the BLPOP client mid-block (its socket goes away;
#      oktoplus's onRead returns EOF, latching theClosed).
#   4. From a fresh client, LPUSH the value the dead client was
#      waiting on.
#   5. From a fresh client, LRANGE the key.
#
# With the fix: LRANGE returns the pushed value (recovery path put
# it back when the dispatched-reply lambda saw theClosed=true).
# Without the fix: LRANGE returns empty -- data lost.
#
# Run:    bash scripts/smoke/test_blpop_disconnect_no_dataloss.sh
# Exits:  0 PASS, 1 FAIL.

set -u

OKTO_BIN="${OKTO_BIN:-$(dirname "$0")/../../build/optimized/src/Executables/Oktoplus/oktoplus}"
REDIS_CLI="${REDIS_CLI:-$HOME/git_store/redis/src/redis-cli}"
PORT="${PORT:-6392}"

if [ ! -x "$OKTO_BIN" ]; then
    echo "FAIL: oktoplus binary not found at $OKTO_BIN" >&2
    exit 1
fi
if [ ! -x "$REDIS_CLI" ]; then
    echo "FAIL: redis-cli not found at $REDIS_CLI" >&2
    exit 1
fi

CONFIG=$(mktemp /tmp/oktoplus_blpop_XXXXXX.json)
LOG=$(mktemp /tmp/oktoplus_blpop_XXXXXX.log)

GRPC_PORT="${GRPC_PORT:-50192}"
cat > "$CONFIG" <<EOF
{
  "service": {
    "endpoint": "0.0.0.0:$GRPC_PORT",
    "numcqs": 4,
    "minpollers": 4,
    "maxpollers": 16,
    "resp_endpoint": "0.0.0.0:$PORT"
  }
}
EOF

"$OKTO_BIN" -c "$CONFIG" > "$LOG" 2>&1 &
OKTO_PID=$!
trap 'kill $OKTO_PID 2>/dev/null; wait $OKTO_PID 2>/dev/null; rm -f "$CONFIG" "$LOG"' EXIT

# Wait for bind.
for i in $(seq 1 50); do
    "$REDIS_CLI" -p "$PORT" PING >/dev/null 2>&1 && break
    sleep 0.1
done
if ! "$REDIS_CLI" -p "$PORT" PING >/dev/null 2>&1; then
    echo "FAIL: oktoplus did not start"
    cat "$LOG" >&2
    exit 1
fi

KEY="dataloss_$$"
"$REDIS_CLI" -p "$PORT" DEL "$KEY" >/dev/null

# Park a BLPOP client in the background with an effectively-infinite
# timeout (10s; we'll kill it well before).
"$REDIS_CLI" -p "$PORT" BLPOP "$KEY" 10 > /dev/null 2>&1 &
BLPOP_PID=$!

# Give the BLPOP time to register the waiter on the storage side.
sleep 0.3

# Kill the BLPOP client: SIGKILL so it doesn't try to QUIT cleanly.
# The kernel will reset the TCP connection. oktoplus's onRead fires
# with EOF, latches theClosed=true.
kill -9 "$BLPOP_PID" 2>/dev/null
wait "$BLPOP_PID" 2>/dev/null

# Give the io_context a beat to process the EOF.
sleep 0.2

# Now push a value onto the same key. With the bug this would wake
# the dead waiter's onWake, hand it the value, the dispatch would
# find conn dead, and the value would be silently dropped.
PUSH_RESULT=$("$REDIS_CLI" -p "$PORT" LPUSH "$KEY" "must_not_be_lost")
echo "LPUSH returned: $PUSH_RESULT"

# Tiny pause so the put-back recovery (if it runs) lands.
sleep 0.1

# Verify the value is still there.
LRANGE_RESULT=$("$REDIS_CLI" -p "$PORT" LRANGE "$KEY" 0 -1)
echo "LRANGE returned: '$LRANGE_RESULT'"

if [ "$LRANGE_RESULT" = "must_not_be_lost" ]; then
    echo "PASS: pushed value survived disconnected-BLPOP race"
    exit 0
else
    echo "FAIL: pushed value lost (data-loss bug regressed)"
    echo "--- server log tail ---"
    tail -30 "$LOG" >&2
    exit 1
fi
