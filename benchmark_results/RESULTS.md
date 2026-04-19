# Oktoplus vs Redis RESP Benchmark Results

**Date:** 2026-04-19
**Build:** Optimized (`-O3 -g3 -DNDEBUG -funroll-loops -ffast-math`)
**Tool:** `redis-benchmark` from Redis 7.x
**Setup:** 100K requests, 100K keyspace (random keys via `-r 100000`)

## Speed: Single Client, No Pipeline

| Command | Redis rps | Oktoplus rps | Ratio |
|---------|-----------|--------------|-------|
| LPUSH | 32,499 | 32,819 | 1.01x |
| RPUSH | 30,665 | 28,960 | 0.94x |
| LPOP | 29,913 | 30,211 | 1.01x |
| RPOP | 32,103 | 29,931 | 0.93x |
| LRANGE_100 | 25,621 | 24,114 | 0.94x |
| LLEN | 30,817 | 30,998 | 1.01x |
| SADD | 33,080 | 30,331 | 0.92x |
| SCARD | 29,949 | 31,114 | 1.04x |

**Takeaway:** At single-client, Oktoplus matches Redis throughput (0.92x-1.04x).
SCARD is slightly faster than Redis; the rest are within noise margin.

## Speed: Single Client, Pipeline=16

| Command | Redis rps | Oktoplus rps | Ratio |
|---------|-----------|--------------|-------|
| LPUSH | 446,429 | N/A | — |
| RPUSH | 337,838 | N/A | — |
| LPOP | 353,357 | N/A | — |
| RPOP | 389,105 | N/A | — |
| LRANGE_100 | 109,769 | N/A | — |
| LLEN | 471,698 | N/A | — |
| SADD | 361,011 | N/A | — |
| SCARD | 438,597 | N/A | — |

**Takeaway:** Pipeline support is not implemented — the RESP handler reads one
command at a time, so pipelining provides no benefit. This is the #1 optimization target.

## Parallelism: Throughput by Client Count (Average across all commands)

| Clients | Redis rps | Oktoplus rps | Ratio |
|---------|-----------|--------------|-------|
| 1 | 30,271 | 30,079 | 0.99x |
| 10 | 80,425 | 72,320 | 0.90x |
| 50 | 92,235 | 68,565 | 0.74x |
| 100 | 99,053 | 68,506 | 0.69x |
| 200 | 81,819 | 68,243 | 0.83x |

## Parallelism: LPUSH Throughput by Client Count

| Clients | Redis rps | Oktoplus rps | Ratio |
|---------|-----------|--------------|-------|
| 1 | 31,017 | 32,884 | 1.06x |
| 10 | 81,037 | 68,776 | 0.85x |
| 50 | 84,317 | 72,569 | 0.86x |
| 100 | 90,171 | 74,019 | 0.82x |
| 200 | 80,775 | 69,300 | 0.86x |

## Parallelism: SADD Throughput by Client Count

| Clients | Redis rps | Oktoplus rps | Ratio |
|---------|-----------|--------------|-------|
| 1 | 29,904 | 30,432 | 1.02x |
| 10 | 80,972 | 68,681 | 0.85x |
| 50 | 94,967 | 70,972 | 0.75x |
| 100 | 113,766 | 68,027 | 0.60x |
| 200 | 81,433 | 70,175 | 0.86x |

**Takeaway:** SADD degrades more steeply under concurrency than list operations.
The outer mutex in ContainerFunctorApplier is the bottleneck — every SADD requires
the map-level lock to find the key's set.

## Optimization Priorities

1. **Pipeline support** — read and batch commands from the socket before dispatching.
   Current: not functional. Target: match single-command throughput x pipeline depth.

2. **Sharded/lock-free key map** — replace the single outer `boost::mutex` in
   `ContainerFunctorApplier` with a sharded map (e.g., 64 shards) to reduce
   contention on key lookup under high concurrency. This would directly address the
   0.60x SADD ratio at 100 clients.

3. **Connection pooling** — replace thread-per-connection with an event loop or
   thread pool + io_uring to reduce thread overhead at high client counts.

4. **Tail latency** — investigate the 196ms max on LRANGE_100 and 59ms max on LPUSH
   at 200 clients. Likely lock convoy or memory allocation stalls.
