# Oktoplus vs Redis RESP Benchmark Results

**Date:** 2026-04-18  
**Build:** Optimized (`-O3 -DNDEBUG -funroll-loops -ffast-math`)  
**Tool:** `redis-benchmark` from Redis 7.x  
**Setup:** 100K requests, 100K keyspace (random keys via `-r 100000`)

## Speed: Single Client, No Pipeline

| Command | Redis rps | Oktoplus rps | Ratio |
|---------|-----------|--------------|-------|
| LPUSH | 32,342 | 32,041 | 0.99x |
| RPUSH | 30,534 | 29,223 | 0.96x |
| LPOP | 28,011 | 25,967 | 0.93x |
| RPOP | 31,928 | 31,143 | 0.97x |
| LLEN | 30,684 | 31,387 | 1.02x |
| LRANGE_100 | 25,284 | 23,524 | 0.93x |
| SADD | 28,810 | 31,676 | 1.10x |
| SCARD | 29,904 | 35,014 | 1.17x |

**Takeaway:** At single-client, Oktoplus matches Redis throughput (0.93x–1.17x).
SADD and SCARD are actually *faster* than Redis.

## Speed: Single Client, Pipeline=16

| Command | Redis rps | Oktoplus rps | Ratio |
|---------|-----------|--------------|-------|
| LPUSH | 398,406 | 391 | 0.001x |
| RPUSH | 374,532 | 391 | 0.001x |
| SADD | 425,532 | 391 | 0.001x |
| LLEN | 446,429 | 391 | 0.001x |

**Takeaway:** Pipeline support is not implemented — the RESP handler reads one
command at a time, so pipelining provides no benefit. This is the #1 optimization target.

## Parallelism: LPUSH Throughput by Client Count

| Clients | Redis rps | Oktoplus rps | Ratio |
|---------|-----------|--------------|-------|
| 1 | 30,675 | 31,776 | 1.04x |
| 10 | 79,936 | 77,519 | 0.97x |
| 50 | 82,237 | 75,019 | 0.91x |
| 100 | 82,440 | 74,963 | 0.91x |
| 200 | 84,674 | 70,028 | 0.83x |

**Takeaway:** LPUSH scales well with concurrency — only modest degradation (0.83x
at 200 clients). The per-key locking prevents complete serialization.

## Parallelism: SADD Throughput by Client Count

| Clients | Redis rps | Oktoplus rps | Ratio |
|---------|-----------|--------------|-------|
| 1 | 29,533 | 32,744 | 1.11x |
| 10 | 80,000 | 70,323 | 0.88x |
| 50 | 91,575 | 67,659 | 0.74x |
| 100 | 82,440 | 42,845 | 0.52x |
| 200 | 85,911 | 32,468 | 0.38x |

**Takeaway:** SADD degrades more steeply under concurrency than list operations.
The outer mutex in ContainerFunctorApplier is the bottleneck — every SADD requires
the map-level lock to find the key's set.

## Optimization Priorities

1. **Pipeline support** — read and batch commands from the socket before dispatching.
   Current: ~390 rps pipelined. Target: match single-command throughput × pipeline depth.

2. **Sharded/lock-free key map** — replace the single outer `boost::mutex` in
   `ContainerFunctorApplier` with a sharded map (e.g., 64 shards) to reduce
   contention on key lookup under high concurrency.

3. **Connection pooling** — replace thread-per-connection with an event loop or
   thread pool + io_uring to reduce thread overhead at high client counts.
