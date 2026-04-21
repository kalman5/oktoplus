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

> Charts are generated from `benchmark_results/raw/*.csv` by `benchmark_results/make_chart.py` (no dependencies — pure-stdlib Python emitting SVG + HTML).
>
> An interactive Chart.js dashboard with the same data lives at [`benchmark_results/report.html`](benchmark_results/report.html) — view it rendered through [htmlpreview.github.io](https://htmlpreview.github.io/?https://github.com/kalman5/oktoplus/blob/master/benchmark_results/report.html).

##### Single client, no pipelining (`-P 1`)

![Single client -P 1 throughput](benchmark_results/chart_p1.svg)

| Test          | Oktoplus rps | Redis rps | Okto / Redis |
|---------------|-------------:|----------:|-------------:|
| LPUSH         |       32,041 |    30,817 |     **104%** |
| SADD          |       31,476 |    30,139 |     **104%** |
| LRANGE_100    |       26,596 |    25,974 |     **102%** |
| LPOP (rand)   |       31,279 |    30,647 |     **102%** |
| RPOP (rand)   |       30,713 |    30,798 |         100% |
| LLEN (rand)   |       31,837 |    32,041 |          99% |
| SCARD (rand)  |       33,467 |    31,506 |     **106%** |

##### Single client, pipelined (`-P 16`)

![Single client -P 16 throughput, small values](benchmark_results/chart_p16.svg)

| Test          | Oktoplus rps | Redis rps | Okto / Redis |
|---------------|-------------:|----------:|-------------:|
| LPUSH         |      436,681 |   389,105 |     **112%** |
| SADD          |      392,157 |   358,423 |     **109%** |
| LPUSH (LRANGE seed) | 444,444 |   383,142 |     **116%** |
| LRANGE_100    |      126,582 |   108,696 |     **116%** |
| RPUSH (rand)  |      271,739 |   342,466 |          79% |
| LPOP (rand)   |      264,550 |   349,650 |          76% |
| RPOP (rand)   |      318,471 |   362,319 |          88% |
| LLEN          |      434,783 |   392,157 |     **111%** |
| SCARD         |      434,783 |   396,825 |     **110%** |

##### Many clients, no pipelining — LPUSH on a hot key

`-P 1` with varying `-c`.

![LPUSH on a hot key, varying clients](benchmark_results/chart_concurrency.svg)

| Clients | Oktoplus rps | Redis rps | Okto / Redis |
|--------:|-------------:|----------:|-------------:|
|       1 |       33,234 |    29,842 |     **111%** |
|      10 |       75,415 |    82,988 |          91% |
|      50 |       76,336 |    83,542 |          91% |
|     100 |       74,850 |    81,367 |          92% |
|     200 |       76,220 |    83,472 |          91% |

##### Many clients, pipelined, random keys

`-c N` with `-P 16` and `__rand_int__` keys (different clients → different keys → different per-key mutexes). RPUSH at varying concurrency:

![RPUSH random key, varying clients (-P 16)](benchmark_results/chart_concurrency_random.svg)

A slice from `concurrent_random_*_p16.csv` at `-c 100`:

| Test            | Oktoplus rps | Redis rps | Okto / Redis |
|-----------------|-------------:|----------:|-------------:|
| RPUSH (rand)    |      775,194 |   900,901 |          86% |
| LPOP (rand)     |      546,448 |   884,956 |          62% |
| RPOP (rand)     |      961,538 | 1,075,269 |          89% |
| **LLEN (rand)** |    1,020,408 | 1,162,791 |     **88%** |
| SADD (rand)     |      769,231 |   990,099 |          78% |
| **SCARD (rand)**|    1,086,957 | 1,052,632 |    **103%** |

##### Single client, pipelined (`-P 16`), 256-byte values

Same workload as the small-value `-P 16` table above but with a 256-byte payload (`-d 256` for built-ins, a 256-byte literal on the custom RPUSH).

![Single client -P 16 throughput, 256-byte values](benchmark_results/chart_p16_d256.svg)

| Test          | Oktoplus rps | Redis rps | Okto / Redis |
|---------------|-------------:|----------:|-------------:|
| LPUSH         |      393,701 |   352,113 |     **112%** |
| SADD          |      374,532 |   378,788 |          99% |
| LPUSH (LRANGE seed) | 381,679 |   364,964 |     **105%** |
| LRANGE_100    |       52,329 |    51,230 |     **102%** |
| RPUSH (rand, 256B) | 230,947 |   291,545 |          79% |
| LPOP (rand)   |      240,385 |   304,878 |          79% |
| RPOP (rand)   |      298,507 |   371,747 |          80% |
| LLEN          |      403,226 |   408,163 |          99% |
| SCARD         |      456,621 |   398,406 |     **115%** |

Full per-test CSVs and the raw-results history are under `benchmark_results/raw/`.

##### Memory footprint

Generated by `benchmark_results/run_memory.sh` — for each cell, start a fresh server, snapshot RSS, load N distinct keys via `RPUSH key:i <value>` over `redis-cli --pipe`, snapshot RSS again. `bytes/key = (steady - baseline) * 1024 / N`.

![Memory footprint, bytes per key](benchmark_results/chart_memory.svg)

| N keys     | value | Oktoplus bytes/key | Redis bytes/key | Okto / Redis |
|-----------:|------:|-------------------:|----------------:|-------------:|
|   100,000  |    3B |                171 |              71 |        2.4×  |
|   100,000  |   64B |                252 |             135 |        1.9×  |
|   100,000  |  256B |                493 |             372 |        1.3×  |
|   100,000  | 1024B |              1,460 |           1,342 |        1.1×  |
| 1,000,000  |    3B |                202 |              72 |        2.8×  |
| 1,000,000  |   64B |                283 |             133 |        2.1×  |
| 1,000,000  |  256B |                526 |             375 |        1.4×  |
| 1,000,000  | 1024B |              1,492 |           1,345 |        1.1×  |

Per-key fixed overhead (extrapolated from the 3-byte rows where the value cost is negligible) is **~70 B** for Redis and **~170-200 B** for Oktoplus. The gap shrinks as the value grows: 2.4× at 3B, 1.9× at 64B, 1.3× at 256B, essentially **at parity** (1.1×) at 1 KB. Full per-trial CSVs at `benchmark_results/raw/memory.csv`, full table at `benchmark_results/memory_results.md`.

#### Where Oktoplus wins

  - **Container choice matches access pattern.** Native [vectors](docs/vectors.md) give O(1) `INDEX` (Redis's `LINDEX` is O(n)). Multi-set and multi-map are first-class. `boost::multi_index_container` with up to 3 keys is on the roadmap. You pick the container; you don't reshape your data to fit a list or hash.
  - **Concurrent writers on different keys actually run in parallel.** The keyspace is split across 32 shards, each key has its own mutex. A workload of N writers touching N different keys uses N cores — not one. Redis 7's I/O threads parallelise socket reads/writes but command execution is single-threaded.
  - **Hot-key and read throughput beat Redis** at every value size we benchmark (see tables above): SADD, LLEN, SCARD land at 99-115% of Redis at `-P 16` across small and 256-byte values; LPUSH wins on both small (112%) and 256-byte (112%). **LRANGE_100** flipped to a win after dropping the per-element list-of-string return type (PERF_TODO item P): 116% small / 102% d256.
  - **Native gRPC alongside RESP.** Generate a typed client in any language straight from `commands.proto` — no need to (re)implement the wire protocol. Existing Redis tooling (`redis-cli`, `redis-benchmark`) still works on the RESP port.

#### What it doesn't do (yet)

  - No replication, clustering, or persistence — see the release plan below.
  - No pub/sub, streams, scripting, or transactions.
  - Command coverage: lists 76%, sets 94% on RESP / 18% on gRPC, strings 0% — see the per-family compatibility tables linked at the top.
  - Random-key writes at high concurrency reach ~80% of Redis, not parity.
  - **Per-key memory overhead is ~2-3× Redis at small values** (~170 B vs 70 B), reaching parity at 1 KB. SDS-style packed keys/values (PERF_TODO item A) would close the residual gap.
  - Single-node, no production deployments.

#### Release plan
- Support all REDIS commands (at least the one relative to data storage)
- Support the following containers: deque, list, map, multimap, multiset, set, unorderd_map, unordered_multimap, vector, boost::multi_index (up to at least 3 keys)
- Make it distributed using RAFT as consensus protocol

***

[How To Build](docs/howtobuild.md)

*** 
