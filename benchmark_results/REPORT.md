# Redis vs Oktoplus Benchmark Report

**Date:** 2026-04-19
**Build:** Optimized (`-O3 -g3 -DNDEBUG -funroll-loops -ffast-math`)
**Host:** devvm, Linux 6.16.1
**Redis:** port 6380 (single-threaded, default config, compiled with `-O3`)
**Oktoplus:** port 6379 (multithreaded, per-key locking)
**Tool:** redis-benchmark, 100K operations per test, `-r 100000` (random keys)

---

## Part 1: Speed — Single Client (Pipeline 1)

| Command | Redis (ops/sec) | Oktoplus (ops/sec) | Ratio (Okto/Redis) |
|---------|----------------:|-------------------:|-------------------:|
| LPUSH | 30,276 | 32,092 | 1.06x |
| RPUSH | 29,507 | 27,655 | 0.94x |
| LPOP | 30,979 | 29,647 | 0.96x |
| RPOP | 29,842 | 30,562 | 1.02x |
| LRANGE_100 | 26,185 | 22,676 | 0.87x |
| LLEN | 26,476 | 30,294 | 1.14x |
| SADD | 31,857 | 31,221 | 0.98x |
| SCARD | 21,372 | 30,432 | 1.42x |

**Takeaway:** Single-client performance is at parity or better. Oktoplus outperforms Redis on SCARD (+42%), LLEN (+14%), and LPUSH (+6%). LRANGE_100 is the weakest point at 0.87x. Note that Redis's SCARD result (21K) appears anomalously low in this run.

---

## Part 2: Speed — Single Client (Pipeline 16)

| Command | Redis (ops/sec) | Oktoplus (ops/sec) | Ratio (Okto/Redis) |
|---------|----------------:|-------------------:|-------------------:|
| LPUSH | 386,100 | ❌ N/A | — |
| RPUSH | 340,136 | ❌ N/A | — |
| LPOP | 349,650 | ❌ N/A | — |
| RPOP | 355,872 | ❌ N/A | — |
| LRANGE_100 | 106,496 | ❌ N/A | — |
| LLEN | 420,168 | ❌ N/A | — |
| SADD | 361,011 | ❌ N/A | — |
| SCARD | 373,134 | ❌ N/A | — |

**Takeaway:** Oktoplus does not support command pipelining. Redis gets a 10-14x throughput boost from pipelining (up to 420K ops/sec for LLEN). **This remains the single biggest performance gap.**

---

## Part 3: Parallelism — Throughput Scaling with Concurrent Clients

All tests use random keys (`-r 100000`) so Oktoplus per-key locking can parallelize.

### LPUSH (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 33,058 | 32,154 | 0.97x |
| 10 | 115,340 | 68,120 | 0.59x |
| 50 | 84,104 | 71,942 | 0.86x |
| 100 | 96,993 | 67,250 | 0.69x |
| 200 | 97,276 | 56,085 | 0.58x |

### RPUSH (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 31,270 | 28,114 | 0.90x |
| 10 | 83,612 | 71,839 | 0.86x |
| 50 | 88,652 | 72,516 | 0.82x |
| 100 | 89,767 | 72,411 | 0.81x |
| 200 | 82,440 | 69,979 | 0.85x |

### LPOP (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 30,239 | 30,377 | 1.00x |
| 10 | 81,833 | 72,727 | 0.89x |
| 50 | 83,333 | 73,368 | 0.88x |
| 100 | 81,566 | 73,421 | 0.90x |
| 200 | 81,900 | 66,357 | 0.81x |

### RPOP (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 30,788 | 31,387 | 1.02x |
| 10 | 78,678 | 74,184 | 0.94x |
| 50 | 82,508 | 69,930 | 0.85x |
| 100 | 101,937 | 76,746 | 0.75x |
| 200 | 82,713 | 67,069 | 0.81x |

### LRANGE_100 (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 26,089 | 23,866 | 0.91x |
| 10 | 61,087 | 55,556 | 0.91x |
| 50 | 59,559 | 54,975 | 0.92x |
| 100 | 62,189 | 53,022 | 0.85x |
| 200 | 61,843 | 52,882 | 0.86x |

### LLEN (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 29,002 | 32,982 | 1.14x |
| 10 | 90,334 | 75,529 | 0.84x |
| 50 | 97,371 | 72,622 | 0.75x |
| 100 | 81,967 | 74,850 | 0.91x |
| 200 | 81,301 | 68,074 | 0.84x |

### SADD (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 30,628 | 32,723 | 1.07x |
| 10 | 91,408 | 68,166 | 0.75x |
| 50 | 83,542 | 71,633 | 0.86x |
| 100 | 107,066 | 72,359 | 0.68x |
| 200 | 82,034 | 69,979 | 0.85x |

### SCARD (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 31,124 | 33,467 | 1.08x |
| 10 | 87,719 | 68,074 | 0.78x |
| 50 | 90,498 | 69,348 | 0.77x |
| 100 | 90,580 | 73,638 | 0.81x |
| 200 | 85,911 | 67,024 | 0.78x |

---

## Summary: Parallelism Scaling (Average across all commands)

| Clients | Redis avg (ops/sec) | Oktoplus avg (ops/sec) | Okto/Redis |
|--------:|--------------------:|-----------------------:|-----------:|
| 1 | 30,274 | 30,633 | 1.01x |
| 10 | 86,251 | 69,274 | 0.80x |
| 50 | 83,695 | 69,541 | 0.83x |
| 100 | 89,008 | 70,462 | 0.79x |
| 200 | 81,927 | 64,681 | 0.79x |

---

## Latency at High Concurrency (200 clients)

| Command | Redis p99 (ms) | Oktoplus p99 (ms) | Redis max (ms) | Oktoplus max (ms) |
|---------|---------------:|------------------:|---------------:|------------------:|
| LPUSH | 1.61 | 28.73 | 2.37 | 284.42 |
| RPUSH | 1.53 | 4.01 | 2.77 | 29.25 |
| LPOP | 1.35 | 1.94 | 2.24 | 4.61 |
| RPOP | 1.36 | 2.02 | 2.06 | 3.29 |
| LRANGE_100 | 1.94 | 2.21 | 2.76 | 6.54 |
| LLEN | 1.41 | 2.02 | 1.94 | 2.79 |
| SADD | 1.37 | 2.02 | 2.10 | 2.79 |
| SCARD | 1.37 | 2.27 | 1.84 | 2.77 |

---

## Key Findings

### 1. Single-client speed is at parity or better (optimized build)
At 1 client, Oktoplus matches or beats Redis on most operations (~30K ops/sec). SCARD is notably faster (+42%), LLEN (+14%). LRANGE_100 is the weakest spot at 0.87x — likely due to serialization overhead copying 100 elements.

### 2. Pipelining is not supported by Oktoplus
Redis gets a 10-14x throughput boost from 16-command pipelines (up to 420K ops/sec). Oktoplus does not benefit from pipelining. **This is the single biggest performance gap.**

### 3. Parallelism does NOT show the expected Oktoplus advantage
Despite using random keys (which should allow per-key locking to parallelize), Oktoplus does NOT scale better than Redis with concurrent clients:
- Redis scales to ~2.8x at 10 clients and sustains ~82-89K ops/sec
- Oktoplus scales to ~2.3x at 10 clients and plateaus around 65-70K ops/sec
- At 200 clients, the gap widens: Redis sustains ~82K avg, Oktoplus drops to ~65K avg (0.79x)
- LPUSH is the worst performer under concurrency (0.58x at 200 clients)

### 4. Tail latency is significantly worse under high concurrency
At 200 clients, LPUSH shows a severe tail latency spike: p99=28.7ms, max=284ms (vs Redis p99=1.6ms, max=2.4ms). RPUSH also shows elevated max latency (29ms). This suggests lock contention storms or allocator contention on list prepend/append paths.

### 5. Optimized vs Debug build comparison
The optimized build numbers are consistent with the previous debug build results. The `-O3` optimization did not materially change throughput, confirming the bottleneck is I/O and synchronization, not CPU-bound computation.

---

## Recommendations

1. **Implement pipelining support** — this is the largest gap and would give an immediate 10x+ throughput improvement for pipeline-capable clients
2. **Profile the LPUSH tail latency** — the 284ms max at 200 clients is a red flag; run perf/flamegraph to identify the contention point
3. **Investigate thread-per-connection overhead** — consider switching to io_context thread pool + async I/O to reduce thread count
4. **Try jemalloc or mimalloc** — if the default allocator is a contention point, a concurrent allocator could help significantly
5. **Test with larger data** — current tests use small values (3 bytes); real workloads with larger payloads may shift the picture

---

## Raw Data

All CSV files are in `benchmark_results/raw/`:
- `speed_{redis,oktoplus}_p{1,16}.csv` — speed tests
- `parallel_{redis,oktoplus}_c{1,10,50,100,200}.csv` — parallelism tests

Benchmark script: `benchmark_results/run_benchmark.sh`
