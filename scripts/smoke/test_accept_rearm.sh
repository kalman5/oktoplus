#!/usr/bin/env bash
# Regression smoke test for the EMFILE accept-rearm fix (commit
# c9c9815). Without that fix a single fd-exhaustion error silently
# killed the listener; this script exercises that exact path and
# verifies the listener is still serving afterwards.
#
# Steps:
#   1. Drop our own ulimit -n so the server (which inherits it) can
#      hit the per-process fd ceiling under modest connection load.
#   2. Start oktoplus on a private port.
#   3. Open a burst of TCP connections to overwhelm the fd budget,
#      then close them. The kernel returns EMFILE / ENFILE /
#      ECONNABORTED to the server's accept() during the burst.
#   4. After the burst settles, send a PING. With the fix it returns
#      PONG (listener re-armed). Without the fix the connection is
#      refused and the script exits non-zero.
#
# Run:    bash scripts/smoke/test_accept_rearm.sh
# Exits:  0 on PASS, 1 on FAIL.
#
# Hard to express as a gtest binary because (a) the test needs to
# drop the per-process fd limit BEFORE the server starts, which
# would affect other tests in the same process, and (b) boost::asio
# doesn't make the acceptor easy to mock. A standalone script with
# its own ulimit -n and process tree is the cleanest fit.

set -u

OKTO_BIN="${OKTO_BIN:-$(dirname "$0")/../../build/optimized/src/Executables/Oktoplus/oktoplus}"
REDIS_CLI="${REDIS_CLI:-$HOME/git_store/redis/src/redis-cli}"
PORT="${PORT:-6391}"
FD_LIMIT="${FD_LIMIT:-64}"
BURST="${BURST:-60}"

if [ ! -x "$OKTO_BIN" ]; then
    echo "FAIL: oktoplus binary not found at $OKTO_BIN" >&2
    echo "      build first or set OKTO_BIN=path/to/oktoplus" >&2
    exit 1
fi
if [ ! -x "$REDIS_CLI" ]; then
    echo "FAIL: redis-cli not found at $REDIS_CLI" >&2
    echo "      set REDIS_CLI=path/to/redis-cli" >&2
    exit 1
fi

# Tight fd ceiling for the server. Has to drop high enough that the
# burst can actually exhaust it but low enough to leave headroom for
# oktoplus's own internals (worker io_contexts, log fd, gRPC, etc.).
ulimit -n "$FD_LIMIT"

CONFIG=$(mktemp /tmp/oktoplus_smoke_XXXXXX.json)
LOG=$(mktemp /tmp/oktoplus_smoke_XXXXXX.log)
trap 'rm -f "$CONFIG" "$LOG"' EXIT

GRPC_PORT="${GRPC_PORT:-50191}"
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

# Wait for the server to bind (max 5s).
for i in $(seq 1 50); do
    if "$REDIS_CLI" -p "$PORT" PING >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done
if ! "$REDIS_CLI" -p "$PORT" PING >/dev/null 2>&1; then
    echo "FAIL: oktoplus did not start within 5s on port $PORT"
    cat "$LOG" >&2
    exit 1
fi

echo "Sanity ping: $("$REDIS_CLI" -p "$PORT" PING)"

# Burst: open then immediately close $BURST sockets in tight succession.
# This is enough to push the server past its fd ceiling repeatedly --
# verified to log "accept error" lines under the fix's
# LOG_EVERY_N(ERROR, 100) throttle.
for i in $(seq 1 "$BURST"); do
    exec 3<>"/dev/tcp/127.0.0.1/$PORT" 2>/dev/null
    exec 3>&-
done

# Give the kernel a beat to free the bursted fds.
sleep 0.5

# THE TEST: did the listener survive?
if RESULT=$("$REDIS_CLI" -p "$PORT" -t 2 PING 2>&1) && [ "$RESULT" = "PONG" ]; then
    LOG_BYTES=$(wc -c < "$LOG")
    ERR_COUNT=$(grep -c "accept error" "$LOG" || true)
    echo "PASS: post-storm PING returned PONG"
    echo "      log size: ${LOG_BYTES} bytes; throttled accept errors logged: ${ERR_COUNT}"
    exit 0
else
    echo "FAIL: post-storm PING failed (\"$RESULT\")"
    echo "      this is the symptom the c9c9815 fix prevents"
    echo "--- server log tail ---"
    tail -20 "$LOG" >&2
    exit 1
fi
