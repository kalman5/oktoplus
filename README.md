# oktoplus

![alt text](docs/octopus-free.png "Oktoplus")

###### What is oktoplus
Oktoplus is a in-memory data store K:V where V is a container: std::list, std::map, boost::multi_index_container, std::set, you name it. Doing so the client can choose the best container for his own access data pattern.

If this reminds you of REDIS then you are right, I was inspired by it, however:

 - Redis is not multithread
 - Redis offers only basic containers
 - For instance the Redis command LINDEX is O(n), so if you need to access a value with an index would be better to use a Vector style container
  - There is no analogue of multi-set in Redis

Redis Commands Compatibility (gRPC / RESP)

  - [LISTS](docs/compatibility_lists.md) — 76% / 76% (16 / 21, blocking variants TBD)
  - [SETS](docs/compatibility_sets.md) — 18% / 94% (3 gRPC, 16 RESP of 17)
  - [STRINGS](docs/compatibility_strings.md) — 0% / 0%

**Oktoplus** specific containers (already implemented, see specific documentation)

  - [DEQUES](docs/deques.md)
  - [VECTORS](docs/vectors.md)

#### Wire protocols

The server exposes the same data through two interfaces:

  - **gRPC** (default port `50051`) — see `src/Libraries/Commands/commands.proto`. Use it to generate a client in your favourite language. Includes admin RPCs `flushAll` / `flushDb` plus all the list / set / deque / vector commands.
  - **RESP** (default port `6379`, optional) — wire-compatible with Redis, so existing tooling like `redis-cli` and `redis-benchmark` works out of the box. Enabled by setting `service.resp_endpoint` in the JSON config. Plus the admin commands `FLUSHDB` / `FLUSHALL`.

The per-family compatibility tables ([LISTS](docs/compatibility_lists.md), [SETS](docs/compatibility_sets.md), [STRINGS](docs/compatibility_strings.md)) include a column showing which Redis commands are wired to gRPC and to RESP today.

Server is multithread, two different clients working on different containers (type or name) have a minimal interaction. For example multiple clients performing a parallel batch insert on different keys can procede in parallel without blocking each other.

#### Benchmarks

A scripted comparison against Redis on the same machine lives at `benchmark_results/` (script: `benchmark_results/run_benchmark.sh`). It starts both servers itself, runs `redis-benchmark` at single-client `-P 1`/`-P 16` and at varying concurrency `-c 1..200`, and dumps CSVs into `benchmark_results/raw/`.

Each `redis-benchmark` invocation runs **N iterations** (env var `ITERATIONS`, default 1; the published numbers below use **N=5**) and the published cell is the **median rps** across them. The harness also flags any test whose `max/min > 1.5×` so we can tell signal from noise — the published random-key cells were noticeably understated by single-run benchmarks because the first iteration captures cold-start costs.

Hardware: AMD EPYC Genoa devserver. Build: `-O3 -march=native -mtune=native -ffast-math -fno-semantic-interposition -funroll-loops`, linked against `jemalloc` (see `OKTOPLUS_WITH_JEMALLOC` in CMake). Workload: 100k ops/iteration, 100k key-space, single client unless stated otherwise.

![Single client -P 16 throughput, small values](benchmark_results/chart_p16.svg)

![Single client -P 16 throughput, 256-byte values](benchmark_results/chart_p16_d256.svg)

![LPUSH on a hot key, varying clients](benchmark_results/chart_concurrency.svg)

> Charts are generated from `benchmark_results/raw/*.csv` by `benchmark_results/make_chart.py` (no dependencies — pure-stdlib Python emitting SVG + HTML).
>
> An interactive Chart.js dashboard with the same data lives at [`benchmark_results/report.html`](benchmark_results/report.html) — view it rendered through [htmlpreview.github.io](https://htmlpreview.github.io/?https://github.com/kalman5/oktoplus/blob/master/benchmark_results/report.html).

##### Single client, no pipelining (`-P 1`) — Oktoplus a hair ahead

At pipeline depth 1 the workload is dominated by the kernel network round-trip, not the server. Both servers land in the same band; Oktoplus edges Redis on the write/scan paths thanks to jemalloc.

| Test          | Oktoplus rps | Redis rps | Okto / Redis |
|---------------|-------------:|----------:|-------------:|
| LPUSH         |       32,144 |    31,114 |     **103%** |
| SADD          |       31,939 |    31,260 |         102% |
| LRANGE_100    |       26,918 |    25,227 |     **107%** |
| LPOP (rand)   |       30,647 |    31,201 |          98% |
| RPOP (rand)   |       30,039 |    31,368 |          96% |
| LLEN (rand)   |       32,020 |    30,826 |         104% |
| SCARD (rand)  |       31,766 |    30,221 |         105% |

##### Single client, pipelined (`-P 16`) — parity on hot key, ~71-81% on random key

Pipelining lets each server stretch its legs. With the RESP parser no longer going through `std::istream`/`std::stoll`, the dispatch table static, the reply path append-into-buffer, `Lists` storage backed by `std::deque` instead of `std::list`, and the binary linked against jemalloc, hot-key throughput is at Redis parity on writes and we **beat Redis** on `SADD` and `SCARD`. Random-key paths trail because each new key still pays an outer-map insert + `ProtectedContainer` allocation (next milestone).

| Test          | Oktoplus rps | Redis rps | Okto / Redis |
|---------------|-------------:|----------:|-------------:|
| LPUSH         |      378,788 |   377,359 |         100% |
| SADD          |      367,647 |   353,357 |     **104%** |
| LPUSH (LRANGE seed) | 401,606 |   401,606 |         100% |
| LRANGE_100    |      107,181 |   109,890 |          98% |
| RPUSH (rand)  |      255,754 |   359,712 |          71% |
| LPOP (rand)   |      261,097 |   346,021 |          75% |
| RPOP (rand)   |      311,526 |   384,615 |          81% |
| LLEN          |      413,223 |   465,116 |          89% |
| SCARD         |      458,716 |   429,185 |     **107%** |

##### Many clients, no pipelining — LPUSH on a hot key

The "parallelism" sweep keeps `-P 1` and varies `-c`. Both servers saturate around 10 clients on a hot key (one TCP connection per client; everything serializes on the inner mutex / single-thread loop respectively).

| Clients | Oktoplus rps | Redis rps | Okto / Redis |
|--------:|-------------:|----------:|-------------:|
|       1 |       32,584 |    30,807 |     **106%** |
|      10 |       73,692 |    80,580 |          91% |
|      50 |       70,126 |    94,340 |          74% |
|     100 |       68,306 |    85,543 |          80% |
|     200 |       70,872 |    82,305 |          86% |

##### Many clients, pipelined, **random keys** — where Oktoplus actually scales

The hot-key sweep above tells you almost nothing about the multithreaded design — every client serialises on the same per-key mutex. To measure what *should* parallelise (different threads → different keys → different inner mutexes), the bench has a separate phase that combines `-c N`, `-P 16`, and `__rand_int__` keys. RPUSH at varying concurrency:

![RPUSH random key, varying clients (-P 16)](benchmark_results/chart_concurrency_random.svg)

A slice from `concurrent_random_*_p16.csv` at `-c 100`:

| Test            | Oktoplus rps | Redis rps | Okto / Redis |
|-----------------|-------------:|----------:|-------------:|
| RPUSH (rand)    |      284,091 |   862,069 |          33% |
| LPOP (rand)     |      366,300 |   862,069 |          42% |
| RPOP (rand)     |      632,911 |  1,063,830 |         59% |
| **LLEN (rand)** |    1,041,667 | 1,123,596 |     **93%** |
| SADD (rand)     |      273,973 |   877,193 |          31% |
| **SCARD (rand)**|    1,020,408 | 1,250,000 |     **82%** |

Where the outer-map work was made cheaper (PERF_TODO items C/D/B), the multithreaded design pulls its weight:

  - **Reads scale to Redis levels.** `LLEN` and `SCARD` at -c 100 are at 93% / 82% of Redis (~1M rps each). Both servers are bound by single-stream processing on this path, but Oktoplus's per-key parallelism keeps it within striking distance.
  - **Writes scale much better than before.** `RPUSH (rand)` at -c 100 was at 19% of Redis when this section first appeared; it's now at **33%** (and the c100 RPOP is at **59%**). The remaining gap is per-key allocation (each new key still pays a `unique_ptr<ProtectedContainer>` heap alloc + outer-map insertion cost) plus thread-per-connection overhead. PERF_TODO item J (async I/O server) is the next big lever.

##### Single client, pipelined (`-P 16`), 256-byte values

Same workload as the small-value `-P 16` table above but the value is padded to 256 bytes (`-d 256` for built-ins, a 256-byte literal on the custom RPUSH). This stresses `std::string` allocation + memcpy + write paths.

| Test          | Oktoplus rps | Redis rps | Okto / Redis |
|---------------|-------------:|----------:|-------------:|
| LPUSH         |      334,448 |   369,004 |          91% |
| SADD          |      393,701 |   377,359 |     **104%** |
| LPUSH (LRANGE seed) | 330,033 |   366,300 |          90% |
| LRANGE_100    |       52,329 |    53,967 |          97% |
| RPUSH (rand, 256B) | 213,220 |   295,858 |          72% |
| LPOP (rand)   |      242,131 |   362,319 |          67% |
| RPOP (rand)   |      330,033 |   374,532 |          88% |
| **LLEN**      |      409,836 |   359,712 |     **114%** |
| **SCARD**     |      438,597 |   373,134 |     **117%** |

Two takeaways:

  - Read-only commands that don't touch the value (`LLEN`, `SCARD`) keep their Redis-or-better margin at any value size.
  - The random-key gap **shrinks slightly** with larger values (RPUSH-rand 71% small → 72% large), confirming the bottleneck is per-*key* overhead (outer-map insert + per-key mutex allocation), not per-value cost. Without the median harness this gap previously looked much wider — the first iteration of `RPUSH ___ val` consistently lands in a cold-cache low (~120K rps), and the median across 5 iterations sits at ~256K.

Full per-test CSVs and the raw-results history are under `benchmark_results/raw/`.

#### Release plan
- Support all REDIS commands (at least the one relative to data storage)
- Support the following containers: deque, list, map, multimap, multiset, set, unorderd_map, unordered_multimap, vector, boost::multi_index (up to at least 3 keys)
- Make it distributed using RAFT as consensus protocol

***

[How To Build](docs/howtobuild.md)

*** 
