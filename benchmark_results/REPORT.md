# Redis vs Oktoplus Benchmark Report

**Date:** 2026-04-19 (re-run with verified optimized build)
**Build:** Optimized (`-O3 -g3 -DNDEBUG -funroll-loops -ffast-math`, GCC 11.5, verified via VERBOSE=1)
**Host:** devvm, Linux 6.16.1, 96 cores
**Redis:** port 6380 (single-threaded, `-O3 -flto`, jemalloc)
**Oktoplus:** port 6379 (multithreaded, per-key locking, gRPC-based RESP)
**Tool:** redis-benchmark, 100K operations per test, `-r 100000` (random keys)

---

## Part 1: Speed — Single Client (Pipeline 1)

| Command | Redis (ops/sec) | Oktoplus (ops/sec) | Ratio (Okto/Redis) |
|---------|----------------:|-------------------:|-------------------:|
| LPUSH | 32,563 | 31,348 | 0.96x |
| RPUSH | 29,913 | 31,417 | 1.05x |
| LPOP | 31,172 | 29,317 | 0.94x |
| RPOP | 29,053 | 31,486 | 1.08x |
| SADD | 30,432 | 29,700 | 0.98x |
| LRANGE_100 | 24,655 | 24,600 | 1.00x |

**Takeaway:** Single-client performance is at parity. All operations within ±8% of Redis. RPOP and RPUSH are slightly faster on Oktoplus; LPOP slightly slower. LRANGE_100 is essentially identical. The optimized build performs at the same level as the previous run, confirming the bottleneck is I/O round-trip latency, not CPU.

---

## Part 2: Speed — Single Client (Pipeline 16)

| Command | Redis (ops/sec) | Oktoplus (ops/sec) | Ratio (Okto/Redis) |
|---------|----------------:|-------------------:|-------------------:|
| LPUSH | 390,625 | 391 | 0.001x |
| RPUSH | 398,406 | 391 | 0.001x |
| LPOP | 393,701 | 391 | 0.001x |
| RPOP | 384,615 | 391 | 0.001x |
| SADD | 364,964 | 391 | 0.001x |
| LRANGE_100 | 108,578 | N/A | — |

**Takeaway:** Oktoplus has a **critical pipelining bug**. Rather than simply not benefiting from pipelining (which would give ~30K ops/sec), it actively degrades to ~391 ops/sec — **80x slower than single-command mode**. The constant 391 rps across all commands suggests Oktoplus is processing entire pipeline batches synchronously with a fixed per-batch overhead (~2.5ms/batch of 16 = ~40ms total per 16-op batch). Redis benefits 10-12x from pipelining, reaching 390K+ ops/sec.

---

## Part 3: Parallelism — Throughput Scaling with Concurrent Clients

All tests use pipeline=1 and random keys (`-r 100000`) so Oktoplus per-key locking can parallelize.

### LPUSH (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 31,476 | 31,990 | 1.02x |
| 10 | 94,251 | 75,586 | 0.80x |
| 50 | 94,518 | 71,582 | 0.76x |
| 100 | 81,103 | 72,939 | 0.90x |
| 200 | 87,566 | 73,475 | 0.84x |

### RPUSH (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 32,830 | 32,310 | 0.98x |
| 10 | 79,745 | 76,220 | 0.96x |
| 50 | 83,056 | 72,150 | 0.87x |
| 100 | 82,988 | 73,746 | 0.89x |
| 200 | 111,359 | 36,738 | 0.33x |

### LPOP (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 33,647 | 34,590 | 1.03x |
| 10 | 80,321 | 76,220 | 0.95x |
| 50 | 81,766 | 75,075 | 0.92x |
| 100 | 83,403 | 73,910 | 0.89x |
| 200 | 105,485 | 72,359 | 0.69x |

### RPOP (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 32,062 | 32,595 | 1.02x |
| 10 | 83,472 | 76,805 | 0.92x |
| 50 | 81,766 | 74,074 | 0.91x |
| 100 | 82,102 | 72,833 | 0.89x |
| 200 | 86,356 | 71,736 | 0.83x |

### SADD (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 30,193 | 31,162 | 1.03x |
| 10 | 89,767 | 79,681 | 0.89x |
| 50 | 82,781 | 73,801 | 0.89x |
| 100 | 80,906 | 71,633 | 0.89x |
| 200 | 82,781 | 71,327 | 0.86x |

### LRANGE_100 (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 25,044 | 24,408 | 0.97x |
| 10 | 72,780 | 57,937 | 0.80x |
| 50 | 61,538 | 53,390 | 0.87x |
| 100 | 58,309 | 55,679 | 0.95x |
| 200 | 60,314 | 52,301 | 0.87x |

---

## Summary: Parallelism Scaling (Average across all commands)

| Clients | Redis avg (ops/sec) | Oktoplus avg (ops/sec) | Okto/Redis |
|--------:|--------------------:|-----------------------:|-----------:|
| 1 | 30,875 | 31,176 | 1.01x |
| 10 | 83,389 | 73,742 | 0.88x |
| 50 | 80,904 | 70,012 | 0.87x |
| 100 | 78,135 | 70,123 | 0.90x |
| 200 | 88,977 | 62,989 | 0.71x |

---

## Latency at High Concurrency (200 clients)

| Command | Redis p99 (ms) | Oktoplus p99 (ms) | Redis max (ms) | Oktoplus max (ms) |
|---------|---------------:|------------------:|---------------:|------------------:|
| LPUSH | 1.34 | 1.94 | 2.47 | 2.60 |
| RPUSH | 1.01 | 70.91 | 1.66 | 544.26 |
| LPOP | 1.30 | 1.90 | 1.66 | 2.54 |
| RPOP | 1.38 | 1.93 | 1.93 | 2.54 |
| SADD | 1.44 | 1.98 | 1.98 | 2.56 |
| LRANGE_100 | 1.98 | 2.22 | 3.06 | 45.57 |

---

## Key Findings

### 1. Single-client speed is at parity (confirmed)
At 1 client/pipeline 1, Oktoplus matches Redis within ±8% across all operations (~30K ops/sec). The optimized build shows no meaningful change from previous results, confirming I/O latency is the bottleneck — not CPU.

### 2. Pipelining is broken, not just unsupported
Oktoplus with pipeline=16 drops to **391 ops/sec** — 80x slower than pipeline=1 (31K). This is not "no benefit from pipelining" — it's **active degradation**. The RESP handler likely processes pipelined commands one-at-a-time with a per-batch synchronization penalty. This is the **#1 priority bug**.

### 3. Parallelism scaling is modest (0.87-0.90x Redis)
Despite multithreading and per-key locking, Oktoplus scales to only ~74K ops/sec at 10+ clients (vs Redis ~83-89K). The gap is consistent across operations. At 200 clients, the average drops to 0.71x due to a catastrophic RPUSH outlier (36K, max latency 544ms).

### 4. RPUSH at 200 clients has a severe contention bug
RPUSH with 200 clients shows: 36,738 ops/sec (vs Redis 111K), p99=70.9ms, max=544ms. All other commands at 200 clients are 71-73K. This suggests a specific lock contention or data structure issue in the RPUSH path under high concurrency. LRANGE_100 also shows elevated max latency (45ms).

### 5. Redis benefits from event loop architecture
Redis's single-threaded event loop avoids all locking overhead. Its 10+ client scaling (~80-110K ops/sec) matches or exceeds Oktoplus's multithreaded approach, suggesting the locking overhead in Oktoplus outweighs the parallelism benefit for these small operations.

---

## Recommendations

1. **Fix the pipelining bug** — this is the #1 priority. Pipelined throughput should be >= single-command throughput. Likely fix: batch-process multiple RESP commands from a single read() without per-command synchronization
2. **Investigate RPUSH contention at 200 clients** — the 544ms max latency and 0.33x ratio suggests a specific bug (lock ordering? allocator contention? resize under contention?)
3. **Profile lock contention** — run `perf lock` or use ThreadSanitizer to identify the contention points causing the 10-15% gap at moderate concurrency
4. **Consider lock-free data structures** — for simple ops like LLEN/SCARD, atomic counters could eliminate locking entirely
5. **Benchmark with larger payloads** — current tests use 3-byte values; real workloads may shift CPU vs I/O balance

---

## Raw Data

All CSV files are in `benchmark_results/raw/`:
- `speed_{redis,oktoplus}_p{1,16}.csv` — speed tests
- `parallel_{redis,oktoplus}_c{1,10,50,100,200}.csv` — parallelism tests

Benchmark script: `benchmark_results/run_benchmark.sh`
