# Redis vs Oktoplus Benchmark Report

**Date:** 2026-04-19
**Build:** Optimized (`-O3 -g3 -DNDEBUG -funroll-loops -ffast-math`)
**Host:** devvm, Linux 6.16.1
**Redis:** port 6380 (single-threaded, default config, built with `-O2`)
**Oktoplus:** port 6379 (multithreaded, per-key locking, optimized build)
**Tool:** redis-benchmark, 100K operations per test, `-r 100000` (random keys)

---

## Part 1: Speed — Single Client (Pipeline 1)

| Command | Redis (ops/sec) | Oktoplus (ops/sec) | Ratio (Okto/Redis) |
|---------|----------------:|-------------------:|-------------------:|
| LPUSH | 32,499 | 32,819 | 1.01x |
| RPUSH | 30,665 | 28,960 | 0.94x |
| LPOP | 29,913 | 30,211 | 1.01x |
| RPOP | 32,103 | 29,931 | 0.93x |
| LRANGE_100 | 25,621 | 24,114 | 0.94x |
| LLEN | 30,817 | 30,998 | 1.01x |
| SADD | 33,080 | 30,331 | 0.92x |
| SCARD | 29,949 | 31,114 | 1.04x |

**Takeaway:** Single-client, single-operation performance is essentially at parity. Oktoplus matches Redis within ~8% on all operations. No measurable overhead from the concurrency infrastructure when running single-threaded.

---

## Part 2: Speed — Single Client (Pipeline 16)

| Command | Redis (ops/sec) | Oktoplus (ops/sec) | Ratio (Okto/Redis) |
|---------|----------------:|-------------------:|-------------------:|
| LPUSH | 446,429 | N/A | — |
| RPUSH | 337,838 | N/A | — |
| LPOP | 353,357 | N/A | — |
| RPOP | 389,105 | N/A | — |
| LRANGE_100 | 109,769 | N/A | — |
| LLEN | 471,698 | N/A | — |
| SADD | 361,011 | N/A | — |
| SCARD | 438,597 | N/A | — |

**Takeaway:** Oktoplus does not support command pipelining. Redis gets a massive 10-14x throughput boost from 16-command pipelines (up to 472K ops/sec). **This is the single biggest performance gap.**

---

## Part 3: Parallelism — Throughput Scaling with Concurrent Clients

All tests use random keys (`-r 100000`) so Oktoplus per-key locking can parallelize.

### LPUSH (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 31,017 | 32,884 | 1.06x |
| 10 | 81,037 | 68,776 | 0.85x |
| 50 | 84,317 | 72,569 | 0.86x |
| 100 | 90,171 | 74,019 | 0.82x |
| 200 | 80,775 | 69,300 | 0.86x |

### RPUSH (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 31,736 | 28,703 | 0.90x |
| 10 | 80,972 | 76,394 | 0.94x |
| 50 | 84,034 | 70,572 | 0.84x |
| 100 | 112,613 | 69,493 | 0.62x |
| 200 | 85,985 | 73,638 | 0.86x |

### LPOP (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 30,553 | 29,577 | 0.97x |
| 10 | 83,126 | 74,294 | 0.89x |
| 50 | 113,895 | 69,493 | 0.61x |
| 100 | 113,766 | 70,323 | 0.62x |
| 200 | 93,545 | 73,260 | 0.78x |

### RPOP (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 31,536 | 30,488 | 0.97x |
| 10 | 97,182 | 74,294 | 0.76x |
| 50 | 114,286 | 69,979 | 0.61x |
| 100 | 112,867 | 70,621 | 0.63x |
| 200 | 81,301 | 72,993 | 0.90x |

### LRANGE_100 (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 26,483 | 24,969 | 0.94x |
| 10 | 58,343 | 56,402 | 0.97x |
| 50 | 74,405 | 54,975 | 0.74x |
| 100 | 66,622 | 52,549 | 0.79x |
| 200 | 59,312 | 48,263 | 0.81x |

### LLEN (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 30,893 | 32,404 | 1.05x |
| 10 | 79,872 | 81,037 | 1.01x |
| 50 | 89,127 | 70,028 | 0.79x |
| 100 | 88,106 | 71,174 | 0.81x |
| 200 | 83,472 | 69,109 | 0.83x |

### SADD (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 29,904 | 30,432 | 1.02x |
| 10 | 80,972 | 68,681 | 0.85x |
| 50 | 94,967 | 70,972 | 0.75x |
| 100 | 113,766 | 68,027 | 0.60x |
| 200 | 81,433 | 70,175 | 0.86x |

### SCARD (ops/sec)

| Clients | Redis | Oktoplus | Ratio (Okto/Redis) |
|--------:|------:|---------:|-------------------:|
| 1 | 30,048 | 31,172 | 1.04x |
| 10 | 81,900 | 78,678 | 0.96x |
| 50 | 82,850 | 69,930 | 0.84x |
| 100 | 94,518 | 71,839 | 0.76x |
| 200 | 88,731 | 69,204 | 0.78x |

---

## Summary: Parallelism Scaling (Average across all commands)

| Clients | Redis avg (ops/sec) | Oktoplus avg (ops/sec) | Okto/Redis |
|--------:|--------------------:|-----------------------:|-----------:|
| 1 | 30,271 | 30,079 | 0.99x |
| 10 | 80,425 | 72,320 | 0.90x |
| 50 | 92,235 | 68,565 | 0.74x |
| 100 | 99,053 | 68,506 | 0.69x |
| 200 | 81,819 | 68,243 | 0.83x |

---

## Key Findings

### 1. Single-client speed is at parity
At 1 client, Oktoplus and Redis are within 1% of each other (~30K ops/sec). No measurable overhead from Oktoplus's concurrency infrastructure when running single-threaded.

### 2. Pipelining is not supported by Oktoplus
Redis gets a 10-14x throughput boost from 16-command pipelines (up to 472K ops/sec). Oktoplus does not benefit from pipelining at all. **This is the single biggest performance gap.**

### 3. Parallelism does NOT show the expected Oktoplus advantage
Despite using random keys (which should allow per-key locking to parallelize), Oktoplus does NOT scale better than Redis with concurrent clients. In fact, **Redis scales significantly better**:
- Redis scales to ~2.7x at 10 clients and peaks at ~99K ops/sec (100 clients)
- Oktoplus scales to ~2.4x at 10 clients and plateaus at ~68-72K ops/sec
- At the sweet spot (50-100 clients), Redis outperforms by 31-44% on average
- The gap is worst on POP operations at 50-100 clients (0.61-0.63x)

### 4. Possible explanations for the parallelism result
- **Lock contention overhead:** Even with per-key locking, the cost of acquiring/releasing locks offsets parallelism benefits. The outer map-level mutex in `ContainerFunctorApplier` may serialize access under load.
- **Thread-per-connection model:** Creating and managing OS threads per connection adds overhead vs Redis's event loop
- **I/O bottleneck:** The RESP handler may have synchronization points that limit throughput
- **CPU saturation:** Both servers may be hitting the CPU limit of the devvm

### 5. Latency profile
At 200 clients, Redis shows tighter tail latencies. Oktoplus has notable outliers:
- LPUSH: Okto max=58.94ms vs Redis max=2.13ms
- LRANGE_100: Okto p99=8.39ms, max=195.97ms vs Redis p99=2.33ms, max=2.85ms
- Other commands are comparable within 2x

---

## Recommendations

1. **Implement pipelining support** — this is the largest gap and would give an immediate 10x+ throughput improvement for pipeline-capable clients
2. **Profile the parallelism bottleneck** — run perf/flamegraph under 50-100 client load. The 0.61x ratio on POP at 50 clients suggests a specific serialization point
3. **Replace thread-per-connection** — use an event loop (io_uring/epoll) + thread pool to reduce thread overhead at high client counts
4. **Investigate tail latency spikes** — the 196ms max on LRANGE_100 and 59ms on LPUSH suggest occasional lock convoy or GC-like pauses

---

## Raw Data

All CSV files are in `benchmark_results/raw/`:
- `speed_{redis,oktoplus}_p{1,16}.csv` — speed tests
- `parallel_{redis,oktoplus}_c{1,10,50,100,200}.csv` — parallelism tests

Benchmark script: `benchmark_results/run_benchmark.sh`
