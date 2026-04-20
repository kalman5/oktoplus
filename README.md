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
| LPUSH         |       33,046 |    31,735 |     **104%** |
| SADD          |       32,299 |    30,211 |     **107%** |
| LRANGE_100    |       26,546 |    25,227 |     **105%** |
| LPOP (rand)   |       30,021 |    30,111 |         100% |
| RPOP (rand)   |       31,887 |    30,978 |     **103%** |
| LLEN (rand)   |       33,489 |    29,985 |     **112%** |
| SCARD (rand)  |       32,862 |    30,978 |     **106%** |

##### Single client, pipelined (`-P 16`) — Oktoplus ahead on hot key + reads, ~80-84% on random-key writes

Pipelining lets each server stretch its legs. With the RESP parser no longer going through `std::istream`/`std::stoll`, the dispatch table static, the reply path append-into-buffer, `Lists` storage backed by `std::deque` instead of `std::list`, the outer keyspace sharded into 32 (mutex, `flat_hash_map`) pairs with embedded inner mutex, the inner-lock holder simplified from `std::optional<unique_lock>` to a plain `unique_lock`, the lock primitives swapped from `boost::mutex`/`recursive_mutex` to plain `std::mutex` (LMOVE same-key rotation now handled in a single critical section so the inner lock no longer needs to be recursive), and the binary linked against jemalloc, Oktoplus now **beats Redis** on every hot-key write path and on `LLEN`/`SCARD`. Random-key writes still trail because each new key pays an outer-map insert + `ProtectedContainer` allocation (next milestone).

| Test          | Oktoplus rps | Redis rps | Okto / Redis |
|---------------|-------------:|----------:|-------------:|
| LPUSH         |      427,350 |   390,624 |     **109%** |
| SADD          |      398,406 |   349,650 |     **114%** |
| LPUSH (LRANGE seed) | 408,163 |   390,624 |     **104%** |
| LRANGE_100    |      109,409 |   108,813 |     **101%** |
| RPUSH (rand)  |      298,507 |   357,142 |          84% |
| LPOP (rand)   |      280,112 |   349,650 |          80% |
| RPOP (rand)   |      334,448 |   395,256 |          85% |
| LLEN          |      434,782 |   411,522 |     **106%** |
| SCARD         |      446,428 |   392,156 |     **114%** |

##### Many clients, no pipelining — LPUSH on a hot key

The "parallelism" sweep keeps `-P 1` and varies `-c`. Both servers saturate around 10 clients on a hot key (one TCP connection per client; everything serializes on the inner mutex / single-thread loop respectively).

| Clients | Oktoplus rps | Redis rps | Okto / Redis |
|--------:|-------------:|----------:|-------------:|
|       1 |       32,289 |    31,545 |     **102%** |
|      10 |       72,568 |    82,918 |          88% |
|      50 |       70,028 |    90,744 |          77% |
|     100 |       68,352 |    83,892 |          81% |
|     200 |       71,736 |    89,445 |          80% |

##### Many clients, pipelined, **random keys** — where Oktoplus actually scales

The hot-key sweep above tells you almost nothing about the multithreaded design — every client serialises on the same per-key mutex. To measure what *should* parallelise (different threads → different keys → different inner mutexes), the bench has a separate phase that combines `-c N`, `-P 16`, and `__rand_int__` keys. RPUSH at varying concurrency:

![RPUSH random key, varying clients (-P 16)](benchmark_results/chart_concurrency_random.svg)

A slice from `concurrent_random_*_p16.csv` at `-c 100`:

| Test            | Oktoplus rps | Redis rps | Okto / Redis |
|-----------------|-------------:|----------:|-------------:|
| RPUSH (rand)    |      714,285 |   900,900 |          79% |
| LPOP (rand)     |      490,196 |   952,381 |          51% |
| RPOP (rand)     |      869,565 | 1,111,111 |          78% |
| **LLEN (rand)** |    1,052,631 | 1,176,470 |     **89%** |
| SADD (rand)     |      675,675 |   980,392 |          69% |
| **SCARD (rand)**|    1,030,927 | 1,250,000 |     **82%** |

Where the outer-map work was made cheaper (PERF_TODO items C/D/B + the `optional<unique_lock>` cleanup + boost→std mutex swap + dropping `recursive_mutex` for plain `std::mutex`), the multithreaded design pulls its weight:

  - **Reads scale to Redis levels.** `SCARD` and `LLEN` at -c 100 are at ~82-89% of Redis (~1M rps each). Both servers are bound by single-stream processing on this path, but Oktoplus's per-key parallelism keeps it within striking distance.
  - **Writes scale dramatically better than before.** `RPUSH (rand)` at -c 100 was at 19% of Redis when this section first appeared; it's now at **79%**. `RPOP (rand)` jumped from 34% to **78%**. `SADD (rand)` from 31% to **69%**. Most of that lift came from the per-call cost — fewer allocations, no `optional<>` indirection on the inner-lock retry, slimmer (and now non-recursive) mutex types — rather than from making the per-key alloc cheaper. The remaining gap is per-key allocation (each new key still pays a `unique_ptr<ProtectedContainer>` heap alloc + outer-map insertion cost) plus thread-per-connection overhead. PERF_TODO item J (async I/O server) is the next big lever.

##### Single client, pipelined (`-P 16`), 256-byte values

Same workload as the small-value `-P 16` table above but the value is padded to 256 bytes (`-d 256` for built-ins, a 256-byte literal on the custom RPUSH). This stresses `std::string` allocation + memcpy + write paths.

| Test          | Oktoplus rps | Redis rps | Okto / Redis |
|---------------|-------------:|----------:|-------------:|
| LPUSH         |      362,318 |   358,422 |     **101%** |
| SADD          |      400,000 |   350,877 |     **114%** |
| LPUSH (LRANGE seed) | 378,787 |   364,963 |     **104%** |
| LRANGE_100    |       53,333 |    55,187 |          97% |
| RPUSH (rand, 256B) | 240,384 |   298,507 |          80% |
| LPOP (rand)   |      255,754 |   332,225 |          77% |
| RPOP (rand)   |      316,455 |   364,963 |          87% |
| LLEN          |      431,034 |   408,163 |     **106%** |
| SCARD         |      454,545 |   390,624 |     **116%** |

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
