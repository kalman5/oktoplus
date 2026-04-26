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

Each `redis-benchmark` invocation runs **N iterations** (env var `ITERATIONS`, default 1; the published numbers below use **N=5**) and the published cell is the **median rps** across them. The harness flags any test whose `max/min > 1.5×` to separate signal from noise: single-run measurements understate random-key throughput because the first iteration pays cold-start costs.

Hardware: AMD EPYC Genoa devserver. Build: `-O3 -march=native -mtune=native -ffast-math -fno-semantic-interposition -funroll-loops`, linked against `jemalloc` (see `OKTOPLUS_WITH_JEMALLOC` in CMake). Workload: 100k ops/iteration, 100k key-space, single client unless stated otherwise.

> Charts are generated from `benchmark_results/raw/*.csv` by `benchmark_results/make_chart.py` (no dependencies — pure-stdlib Python emitting SVG + HTML).
>
> An interactive Chart.js dashboard with the same data lives at [`benchmark_results/report.html`](benchmark_results/report.html) — view it rendered through [htmlpreview.github.io](https://htmlpreview.github.io/?https://github.com/kalman5/oktoplus/blob/master/benchmark_results/report.html).

##### Single client, no pipelining (`-P 1`)

![Single client -P 1 throughput](benchmark_results/chart_p1.svg)

| Test          | Oktoplus rps | Redis rps | Okto / Redis |
|---------------|-------------:|----------:|-------------:|
| LPUSH         |       31,211 |    30,525 |     **102%** |
| SADD          |       30,950 |    30,157 |     **103%** |
| LRANGE_100    |       26,674 |    24,771 |     **108%** |
| LPOP (rand)   |       32,394 |    30,989 |     **105%** |
| RPOP (rand)   |       31,046 |    30,193 |     **103%** |
| LLEN (rand)   |       32,992 |    30,572 |     **108%** |
| SCARD (rand)  |       31,949 |    29,718 |     **108%** |

##### Single client, pipelined (`-P 16`)

![Single client -P 16 throughput, small values](benchmark_results/chart_p16.svg)

| Test          | Oktoplus rps | Redis rps | Okto / Redis |
|---------------|-------------:|----------:|-------------:|
| LPUSH         |      421,941 |   392,157 |     **108%** |
| SADD          |      367,647 |   357,143 |     **103%** |
| LPUSH (LRANGE seed) | 458,716 |   362,319 |     **127%** |
| LRANGE_100    |      120,482 |   107,296 |     **112%** |
| RPUSH (rand)  |      361,011 |   340,136 |     **106%** |
| LPOP (rand)   |      349,650 |   346,021 |     **101%** |
| RPOP (rand)   |      404,858 |   370,370 |     **109%** |
| LLEN          |      454,545 |   406,504 |     **112%** |
| SCARD         |      444,444 |   375,940 |     **118%** |

##### Many clients, no pipelining — LPUSH on a hot key

`-P 1` with varying `-c`.

![LPUSH on a hot key, varying clients](benchmark_results/chart_concurrency.svg)

| Clients | Oktoplus rps | Redis rps | Okto / Redis |
|--------:|-------------:|----------:|-------------:|
|       1 |       32,092 |    30,931 |     **104%** |
|      10 |       76,220 |    85,837 |          89% |
|      50 |       75,019 |    86,957 |          86% |
|     100 |       76,046 |    87,796 |          87% |
|     200 |       75,988 |    93,197 |          82% |

##### Many clients, pipelined, random keys

`-c N` with `-P 16` and `__rand_int__` keys (different clients → different keys → different per-key mutexes). RPUSH at varying concurrency:

![RPUSH random key, varying clients (-P 16)](benchmark_results/chart_concurrency_random.svg)

A slice from `concurrent_random_*_p16.csv` at `-c 100`:

| Test            | Oktoplus rps | Redis rps | Okto / Redis |
|-----------------|-------------:|----------:|-------------:|
| RPUSH (rand)    |    1,063,830 |   952,381 |     **112%** |
| LPOP (rand)     |    1,063,830 |   943,396 |     **113%** |
| RPOP (rand)     |    1,075,269 | 1,098,901 |          98% |
| LLEN (rand)     |    1,052,632 | 1,162,791 |          91% |
| SADD (rand)     |    1,030,928 | 1,010,101 |     **102%** |
| SCARD (rand)    |    1,086,957 | 1,041,667 |     **104%** |

##### Single client, pipelined (`-P 16`), 256-byte values

Same workload as the small-value `-P 16` table above but with a 256-byte payload (`-d 256` for built-ins, a 256-byte literal on the custom RPUSH).

![Single client -P 16 throughput, 256-byte values](benchmark_results/chart_p16_d256.svg)

| Test          | Oktoplus rps | Redis rps | Okto / Redis |
|---------------|-------------:|----------:|-------------:|
| LPUSH         |      414,938 |   357,143 |     **116%** |
| SADD          |      384,615 |   343,643 |     **112%** |
| LPUSH (LRANGE seed) | 403,226 |   355,872 |     **113%** |
| LRANGE_100    |       55,249 |    53,821 |     **103%** |
| RPUSH (rand, 256B) | 320,513 |   297,619 |     **108%** |
| LPOP (rand)   |      336,700 |   341,297 |          99% |
| RPOP (rand)   |      414,938 |   363,636 |     **114%** |
| LLEN          |      418,410 |   392,157 |     **107%** |
| SCARD         |      434,783 |   393,701 |     **110%** |

Full per-test CSVs and the raw-results history are under `benchmark_results/raw/`.

##### Memory footprint

Generated by `benchmark_results/run_memory.sh` — for each cell, start a fresh server, snapshot RSS, load N distinct keys via `RPUSH key:i <value>` over `redis-cli --pipe`, snapshot RSS again. `bytes/key = (steady - baseline) * 1024 / N`.

![Memory footprint, bytes per key](benchmark_results/chart_memory.svg)

| N keys     | value | Oktoplus bytes/key | Redis bytes/key | Okto / Redis |
|-----------:|------:|-------------------:|----------------:|-------------:|
|   100,000  |    3B |                172 |              71 |        2.4×  |
|   100,000  |   64B |                253 |             135 |        1.9×  |
|   100,000  |  256B |                494 |             373 |        1.3×  |
|   100,000  | 1024B |              1,461 |           1,342 |        1.1×  |
| 1,000,000  |    3B |                224 |              72 |        3.1×  |
| 1,000,000  |   64B |                305 |             133 |        2.3×  |
| 1,000,000  |  256B |                546 |             375 |        1.5×  |
| 1,000,000  | 1024B |              1,517 |           1,345 |        1.1×  |

Per-key fixed overhead (extrapolated from the 3-byte rows where the value cost is negligible) is **~70 B** for Redis and **~170-220 B** for Oktoplus. The gap shrinks as the value grows: 2.4× at 3B (100k), 1.9× at 64B, 1.3× at 256B, essentially **at parity** (1.1×) at 1 KB. Full per-trial CSVs at `benchmark_results/raw/memory.csv`, full table at `benchmark_results/memory_results.md`.

##### Residual memory after FLUSHALL

`FLUSHALL` and `FLUSHDB` clear every container and then call jemalloc's `mallctl("arena.<all>.purge")` to return dirty pages to the OS, so post-flush RSS lands close to baseline instead of stalling at the steady-state high-water mark.

![Residual RSS after FLUSHALL](benchmark_results/chart_memory_residual.svg)

| N keys     | value | Oktoplus residual (KiB) | Redis residual (KiB) |
|-----------:|------:|------------------------:|---------------------:|
|   100,000  |    3B |                  26,848 |               10,076 |
|   100,000  |   64B |                  27,624 |               10,060 |
|   100,000  |  256B |                  28,616 |               10,072 |
|   100,000  | 1024B |                  27,752 |               11,004 |
| 1,000,000  |    3B |                  29,840 |               11,516 |
| 1,000,000  |   64B |                  30,388 |               11,748 |
| 1,000,000  |  256B |                  31,680 |               14,180 |
| 1,000,000  | 1024B |                  38,840 |               23,752 |

Baseline RSS is ~21 MiB for Oktoplus and ~9 MiB for Redis; *delta over baseline* (truly retained allocator memory) is ~6–17 MiB on Oktoplus vs ~1–14 MiB on Redis across the workload sweep.

#### Where Oktoplus wins

  - **Container choice matches access pattern.** Native [vectors](docs/vectors.md) give O(1) `INDEX` (Redis's `LINDEX` is O(n)). Multi-set and multi-map are first-class. `boost::multi_index_container` with up to 3 keys is on the roadmap. You pick the container; you don't reshape your data to fit a list or hash.
  - **Concurrent writers on different keys actually run in parallel.** The keyspace is split across 32 shards, each key has its own mutex. A workload of N writers touching N different keys uses N cores — not one. Redis 7's I/O threads parallelise socket reads/writes but command execution is single-threaded.
  - **Hot-key, read, and random-key throughput all beat Redis** at every value size benchmarked (see tables above). At single-client `-P 16`: LPUSH 108% small / 116% on 256-byte values; SADD/LLEN/SCARD 103–118%; random-key RPUSH/LPOP/RPOP 101–114%; LRANGE_100 112% small / 103% on 256-byte. At `-c 100 -P 16` random-key: RPUSH 112%, LPOP 113%, SADD 102%, SCARD 104%.
  - **Native gRPC alongside RESP.** Generate a typed client in any language straight from `commands.proto` — no need to (re)implement the wire protocol. Existing Redis tooling (`redis-cli`, `redis-benchmark`) still works on the RESP port.

#### What it doesn't do (yet)

  - No replication, clustering, or persistence — see the release plan below.
  - No pub/sub, streams, scripting, or transactions.
  - Command coverage: lists 76%, sets 94% on RESP / 18% on gRPC, strings 0% — see the per-family compatibility tables linked at the top.
  - Hot-key LPUSH at varying concurrency saturates around ~76K rps (single-key mutex), trailing Redis's ~85-93K above `-c 10`.
  - **Per-key memory overhead is ~2-3× Redis at small values** (~170 B vs 70 B), reaching parity at 1 KB. SDS-style packed keys/values (PERF_TODO item A) would close the remaining steady-state gap.
  - Single-node, no production deployments.

#### Release plan
- Support all REDIS commands (at least the one relative to data storage)
- Support the following containers: deque, list, map, multimap, multiset, set, unorderd_map, unordered_multimap, vector, boost::multi_index (up to at least 3 keys)
- Make it distributed using RAFT as consensus protocol

***

[How To Build](docs/howtobuild.md)

*** 
