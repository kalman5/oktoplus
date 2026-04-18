# Redis vs Oktoplus Benchmark Report

**Date:** 2026-04-18
**Host:** devvm, Linux 6.16.1
**Redis:** port 6380 (single-threaded, default config)
**Oktoplus:** port 6379 (multithreaded, per-key locking)
**Tool:** redis-benchmark, 100K operations per test, `-r 100000` (random keys)

---

## Part 1: Speed — Single Client (Pipeline 1)

| Command | Redis (ops/sec) | Oktoplus (ops/sec) | Ratio (Okto/Redis) |
|---------|----------------:|-------------------:|-------------------:|
| LPUSH | 32,342 | 32,041 | 0.99x |
| RPUSH | 30,534 | 29,223 | 0.96x |
| LPOP | 28,011 | 25,967 | 0.93x |
| RPOP | 31,928 | 31,143 | 0.98x |
| LRANGE_100 | 25,284 | 23,524 | 0.93x |
| LLEN | 30,684 | 31,387 | 1.02x |
| SADD | 28,810 | 31,676 | 1.10x |
| SCARD | 29,904 | 35,014 | 1.17x |

**Takeaway:** Single-client, single-operation performance is essentially on par. Oktoplus matches Redis within ~7% on list operations and slightly outperforms on set metadata operations (SCARD, SADD).

---

## Part 2: Speed — Single Client (Pipeline 16)

| Command | Redis (ops/sec) | Oktoplus (ops/sec) | Ratio (Okto/Redis) |
|---------|----------------:|-------------------:|-------------------:|
| LPUSH | 398,406 | ❌ N/A | — |
| RPUSH | 374,532 | ❌ N/A | — |
| LPOP | 363,636 | ❌ N/A | — |
| RPOP | 377,359 | ❌ N/A | — |
| LRANGE_100 | 108,578 | ❌ N/A | — |
| LLEN | 446,429 | ❌ N/A | — |
| SADD | 425,532 | ❌ N/A | — |
| SCARD | 387,597 | ❌ N/A | — |

**Takeaway:** Oktoplus does not support command pipelining (tested manually: drops to ~400 ops/sec with `-P 16`, suggesting it processes the entire pipeline as one operation). Redis gets a massive 10-14x throughput boost from pipelining. **This is a significant gap** — pipelining is a critical optimization for real-world Redis clients.

---

## Part 3: Parallelism — Throughput Scaling with Concurrent Clients

All tests use random keys (`-r 100000`) so Oktoplus per-key locking can parallelize.

### LPUSH (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 31,476 | 31,546 | 1.00x |
| 10 | 81,235 | 71,582 | 0.88x |
| 50 | 98,135 | 77,160 | 0.79x |
| 100 | 104,603 | 75,873 | 0.73x |
| 200 | 80,972 | 78,125 | 0.97x |

### RPUSH (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 29,248 | 27,367 | 0.94x |
| 10 | 80,321 | 75,529 | 0.94x |
| 50 | 83,264 | 74,239 | 0.89x |
| 100 | 83,612 | 70,373 | 0.84x |
| 200 | 84,459 | 74,349 | 0.88x |

### LPOP (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 32,873 | 31,027 | 0.94x |
| 10 | 88,339 | 72,939 | 0.83x |
| 50 | 83,612 | 71,480 | 0.85x |
| 100 | 75,758 | 70,922 | 0.94x |
| 200 | 82,645 | 71,788 | 0.87x |

### RPOP (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 31,908 | 29,621 | 0.93x |
| 10 | 83,056 | 72,727 | 0.88x |
| 50 | 90,334 | 72,674 | 0.80x |
| 100 | 76,687 | 70,028 | 0.91x |
| 200 | 82,645 | 71,429 | 0.86x |

### LRANGE_100 (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 25,654 | 24,390 | 0.95x |
| 10 | 59,524 | 55,617 | 0.93x |
| 50 | 69,204 | 58,106 | 0.84x |
| 100 | 63,532 | 52,798 | 0.83x |
| 200 | 57,307 | 47,281 | 0.82x |

### LLEN (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 31,240 | 33,245 | 1.06x |
| 10 | 93,023 | 72,993 | 0.78x |
| 50 | 84,602 | 72,359 | 0.86x |
| 100 | 74,850 | 69,686 | 0.93x |
| 200 | 82,988 | 78,802 | 0.95x |

### SADD (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 31,075 | 32,321 | 1.04x |
| 10 | 81,433 | 70,771 | 0.87x |
| 50 | 106,157 | 82,440 | 0.78x |
| 100 | 82,508 | 72,202 | 0.88x |
| 200 | 80,386 | 76,453 | 0.95x |

### SCARD (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 32,648 | 33,036 | 1.01x |
| 10 | 79,618 | 72,359 | 0.91x |
| 50 | 92,166 | 71,633 | 0.78x |
| 100 | 75,358 | 71,073 | 0.94x |
| 200 | 80,515 | 81,103 | 1.01x |

---

## Summary: Parallelism Scaling (Average across all commands)

| Clients | Redis avg (ops/sec) | Oktoplus avg (ops/sec) | Okto/Redis |
|--------:|--------------------:|-----------------------:|-----------:|
| 1 | 30,515 | 30,319 | 0.99x |
| 10 | 80,819 | 70,502 | 0.87x |
| 50 | 88,372 | 72,474 | 0.82x |
| 100 | 79,688 | 69,095 | 0.87x |
| 200 | 79,115 | 72,542 | 0.92x |

---

## Key Findings

### 1. Single-client speed is at parity
At 1 client, Oktoplus and Redis are within 1% of each other (~30K ops/sec). No measurable overhead from Oktoplus's concurrency infrastructure when running single-threaded.

### 2. Pipelining is not supported by Oktoplus
Redis gets a 10-14x throughput boost from 16-command pipelines (up to 450K ops/sec). Oktoplus does not benefit from pipelining at all. **This is the single biggest performance gap.**

### 3. Parallelism does NOT show the expected Oktoplus advantage
Despite using random keys (which should allow per-key locking to parallelize), Oktoplus does NOT scale better than Redis with concurrent clients. In fact, **Redis scales slightly better** in most cases:
- Redis scales to ~2.5-3.4x at 10 clients and plateaus around 80-100K ops/sec
- Oktoplus scales to ~2.2-2.4x at 10 clients and plateaus around 70-80K ops/sec
- Both plateau at similar concurrency levels, but Redis peaks ~10-20% higher

### 4. Possible explanations for the parallelism result
- **Lock contention overhead:** Even with per-key locking, the cost of acquiring/releasing locks may offset the parallelism benefit
- **Internal bottleneck:** There may be a shared resource (memory allocator, I/O thread, event loop) that serializes under load
- **redis-benchmark limitation:** The benchmark tool generates synthetic RESP traffic where the bottleneck may be client-side or network-side rather than server-side
- **CPU saturation:** Both servers may be hitting the CPU limit of the devvm

### 5. Latency profile
At high concurrency (200 clients), both servers show similar p50 latency (~1.2-1.4ms). Oktoplus has slightly worse tail latency on LRANGE_100 (p99=10.99ms, max=212ms vs Redis p99=2.31ms, max=3.48ms).

---

## Recommendations

1. **Implement pipelining support** — this is the largest gap and would give an immediate 10x+ throughput improvement for pipeline-capable clients
2. **Profile the parallelism bottleneck** — run perf/flamegraph under 50-200 client load to identify what's limiting scaling
3. **Consider a different benchmark approach** — use a custom multithreaded client that pre-creates independent key spaces and measures server-side metrics, to rule out client-side bottlenecks in redis-benchmark
4. **Test with larger data** — current tests use small values (3 bytes); real workloads with larger payloads may shift the picture

---

## Raw Data

All CSV files are in `benchmark_results/raw/`:
- `speed_{redis,oktoplus}_p{1,16}.csv` — speed tests
- `parallel_{redis,oktoplus}_c{1,10,50,100,200}.csv` — parallelism tests

Benchmark script: `benchmark_results/run_benchmark.sh`
