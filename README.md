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
| LPUSH         |       33,760 |    31,705 |     **107%** |
| SADD          |       34,153 |    31,172 |     **110%** |
| LRANGE_100    |       26,497 |    25,588 |     **104%** |
| LPOP (rand)   |       30,184 |    30,395 |          99% |
| RPOP (rand)   |       30,902 |    30,459 |     **101%** |
| LLEN (rand)   |       33,534 |    30,864 |     **109%** |
| SCARD (rand)  |       32,615 |    30,432 |     **107%** |

##### Single client, pipelined (`-P 16`) — Oktoplus ahead on hot key + reads, ~72-86% on random-key writes

Pipelining lets each server stretch its legs. With the RESP parser no longer going through `std::istream`/`std::stoll`, the dispatch table static, the reply path append-into-buffer, `Lists` storage backed by `std::deque` instead of `std::list`, the outer keyspace sharded into 32 (mutex, `flat_hash_map`) pairs with embedded inner mutex, the inner-lock holder simplified from `std::optional<unique_lock>` to a plain `unique_lock`, the lock primitives swapped from `boost::mutex`/`recursive_mutex` to their `std::` equivalents, and the binary linked against jemalloc, Oktoplus now **beats Redis** on every hot-key write path and on `LLEN`/`SCARD`. Random-key writes still trail because each new key pays an outer-map insert + `ProtectedContainer` allocation (next milestone).

| Test          | Oktoplus rps | Redis rps | Okto / Redis |
|---------------|-------------:|----------:|-------------:|
| LPUSH         |      403,225 |   387,596 |     **104%** |
| SADD          |      369,003 |   342,465 |     **108%** |
| LPUSH (LRANGE seed) | 429,184 |   392,156 |     **109%** |
| LRANGE_100    |      106,269 |   108,695 |          98% |
| RPUSH (rand)  |      283,286 |   337,837 |          84% |
| LPOP (rand)   |      253,164 |   350,877 |          72% |
| RPOP (rand)   |      313,479 |   366,300 |          86% |
| LLEN          |      458,715 |   406,504 |     **113%** |
| SCARD         |      469,483 |   398,406 |     **118%** |

##### Many clients, no pipelining — LPUSH on a hot key

The "parallelism" sweep keeps `-P 1` and varies `-c`. Both servers saturate around 10 clients on a hot key (one TCP connection per client; everything serializes on the inner mutex / single-thread loop respectively).

| Clients | Oktoplus rps | Redis rps | Okto / Redis |
|--------:|-------------:|----------:|-------------:|
|       1 |       32,959 |    31,094 |     **106%** |
|      10 |       73,691 |    81,699 |          90% |
|      50 |       68,917 |    87,260 |          79% |
|     100 |       72,780 |    92,936 |          78% |
|     200 |       75,075 |    81,168 |          92% |

##### Many clients, pipelined, **random keys** — where Oktoplus actually scales

The hot-key sweep above tells you almost nothing about the multithreaded design — every client serialises on the same per-key mutex. To measure what *should* parallelise (different threads → different keys → different inner mutexes), the bench has a separate phase that combines `-c N`, `-P 16`, and `__rand_int__` keys. RPUSH at varying concurrency:

![RPUSH random key, varying clients (-P 16)](benchmark_results/chart_concurrency_random.svg)

A slice from `concurrent_random_*_p16.csv` at `-c 100`:

| Test            | Oktoplus rps | Redis rps | Okto / Redis |
|-----------------|-------------:|----------:|-------------:|
| RPUSH (rand)    |      694,444 |   892,857 |          78% |
| LPOP (rand)     |      487,804 |   934,579 |          52% |
| RPOP (rand)     |      806,451 | 1,052,631 |          77% |
| **LLEN (rand)** |    1,041,666 | 1,149,425 |     **91%** |
| SADD (rand)     |      671,140 |   970,873 |          69% |
| **SCARD (rand)**|    1,052,631 | 1,063,829 |     **99%** |

Where the outer-map work was made cheaper (PERF_TODO items C/D/B + the `optional<unique_lock>` cleanup + boost→std mutex swap), the multithreaded design pulls its weight:

  - **Reads scale to Redis levels.** `SCARD` at -c 100 is at **99%** of Redis (essentially tied at ~1.05M rps each); `LLEN` at 91%. Both servers are bound by single-stream processing on this path, but Oktoplus's per-key parallelism keeps it within striking distance.
  - **Writes scale dramatically better than before.** `RPUSH (rand)` at -c 100 was at 19% of Redis when this section first appeared; it's now at **78%**. `RPOP (rand)` jumped from 34% to **77%**. `SADD (rand)` from 31% to **69%**. Most of that lift came from the per-call cost — fewer allocations, no `optional<>` indirection on the inner-lock retry, slimmer mutex types — rather than from making the per-key alloc cheaper. The remaining gap is per-key allocation (each new key still pays a `unique_ptr<ProtectedContainer>` heap alloc + outer-map insertion cost) plus thread-per-connection overhead. PERF_TODO item J (async I/O server) is the next big lever.

##### Single client, pipelined (`-P 16`), 256-byte values

Same workload as the small-value `-P 16` table above but the value is padded to 256 bytes (`-d 256` for built-ins, a 256-byte literal on the custom RPUSH). This stresses `std::string` allocation + memcpy + write paths.

| Test          | Oktoplus rps | Redis rps | Okto / Redis |
|---------------|-------------:|----------:|-------------:|
| LPUSH         |      344,827 |   359,712 |          96% |
| SADD          |      406,504 |   358,422 |     **113%** |
| LPUSH (LRANGE seed) | 369,003 |   358,422 |     **103%** |
| LRANGE_100    |       53,078 |    54,259 |          98% |
| RPUSH (rand, 256B) | 236,966 |   322,580 |          73% |
| LPOP (rand)   |      243,309 |   323,624 |          75% |
| RPOP (rand)   |      312,500 |   371,747 |          84% |
| LLEN          |      420,168 |   409,836 |     **103%** |
| SCARD         |      400,000 |   393,700 |     **102%** |

Two takeaways:

  - Read-only commands that don't touch the value (`LLEN`, `SCARD`) hold parity (or better) at 256-byte values too.
  - The random-key gap is essentially the same with 256-byte values as with small values (RPUSH-rand 84% small → 73% large at single-client, but ~75% on both for LPOP), confirming the bottleneck is per-*key* overhead (outer-map insert + per-key mutex allocation), not per-value cost. Without the median harness this gap previously looked much wider — the first iteration of `RPUSH ___ val` consistently lands in a cold-cache low (~120K rps), and the median across 5 iterations smooths it out.

Full per-test CSVs and the raw-results history are under `benchmark_results/raw/`.

#### Release plan
- Support all REDIS commands (at least the one relative to data storage)
- Support the following containers: deque, list, map, multimap, multiset, set, unorderd_map, unordered_multimap, vector, boost::multi_index (up to at least 3 keys)
- Make it distributed using RAFT as consensus protocol

***

[How To Build](docs/howtobuild.md)

*** 
