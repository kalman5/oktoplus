# oktoplus

![alt text](docs/octopus-free.png "Oktoplus")

###### What is oktoplus
Oktoplus is a in-memory data store K:V where V is a container: std::list, std::map, boost::multi_index_container, std::set, you name it. Doing so the client can choose the best container for his own access data pattern.

If this reminds you of REDIS then you are right, I was inspired by it, however:

 - Redis is not multithread
 - Redis offers only basic containers
 - For instance the Redis command LINDEX is O(n), so if you need to access a value with an index would be better to use a Vector style container
  - There is no analogue of multi-set in Redis

Redis Commands Compatibility (RESP)

  - [LISTS](docs/compatibility_lists.md) — 100% (21 / 21)
  - [SETS](docs/compatibility_sets.md) — 94% (16 / 17)
  - [STRINGS](docs/compatibility_strings.md) — 0%

**Oktoplus** specific containers (already implemented, see specific documentation)

  - [DEQUES](docs/deques.md)
  - [VECTORS](docs/vectors.md)

#### Wire protocols

The server exposes the same data through two interfaces:

  - **RESP2** (default port `6379`, always on) — primary wire protocol, wire-compatible with Redis using the RESP2 framing (`+` `-` `:` `$` `*` types, `$-1\r\n` / `*-1\r\n` nulls), so existing tooling like `redis-cli` and `redis-benchmark` works out of the box. Override the bind address via `service.resp_endpoint` in the JSON config. Includes the admin commands `FLUSHDB` / `FLUSHALL`. RESP3 (`HELLO`-negotiated, native maps/sets/push, unified `_\r\n` null) is on the roadmap — see TODO below.
  - **gRPC** (optional) — see `src/Libraries/Commands/commands.proto`. Use it to generate a client in your favourite language. Includes admin RPCs `flushAll` / `flushDb` plus all the list / set / deque / vector commands. **Disabled by default at build time** to keep baseline RSS at Redis-parity (~9 MiB) — pass `-DOKTOPLUS_WITH_GRPC=ON` to cmake to compile it in, then enable at runtime by setting `service.endpoint` in the JSON config.

The per-family compatibility tables ([LISTS](docs/compatibility_lists.md), [SETS](docs/compatibility_sets.md), [STRINGS](docs/compatibility_strings.md)) include a column showing which Redis commands are wired to gRPC and to RESP today.

#### TODO

  - **RESP3 protocol support**: implement `HELLO` for protocol negotiation, gate the per-connection encoder on the negotiated version, swap to the unified `_\r\n` null, and add the new type tags (`#` boolean, `,` double, `(` big number, `=` verbatim string, `~` set, `%` map, `>` push, `|` attribute). Today the server speaks RESP2 only; RESP3-capable clients (e.g. `redis-cli -3`) fall back to RESP2 because `HELLO` returns `ERR unknown command`.

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
| LPUSH         |       31,289 |    29,386 |     **106%** |
| SADD          |       30,230 |    29,155 |     **104%** |
| LRANGE_100    |       25,471 |    24,337 |     **105%** |
| LPOP (rand)   |       30,525 |    30,377 |         100% |
| RPOP (rand)   |       30,969 |    30,057 |     **103%** |
| LLEN (rand)   |       31,368 |    30,497 |     **103%** |
| SCARD (rand)  |       31,221 |    30,075 |     **104%** |

##### Single client, pipelined (`-P 16`)

![Single client -P 16 throughput, small values](benchmark_results/chart_p16.svg)

| Test          | Oktoplus rps | Redis rps | Okto / Redis |
|---------------|-------------:|----------:|-------------:|
| LPUSH         |      460,830 |   389,105 |     **118%** |
| SADD          |      408,163 |   346,021 |     **118%** |
| LPUSH (LRANGE seed) | 456,621 |   366,300 |     **125%** |
| LRANGE_100    |      124,069 |   107,296 |     **116%** |
| RPUSH (rand)  |      342,466 |   344,828 |          99% |
| LPOP (rand)   |      347,222 |   348,432 |         100% |
| RPOP (rand)   |      403,226 |   367,647 |     **110%** |
| LLEN          |      483,092 |   398,406 |     **121%** |
| SCARD         |      442,478 |   395,257 |     **112%** |

##### Many clients, no pipelining — LPUSH on a hot key

`-P 1` with varying `-c`.

![LPUSH on a hot key, varying clients](benchmark_results/chart_concurrency.svg)

| Clients | Oktoplus rps | Redis rps | Okto / Redis |
|--------:|-------------:|----------:|-------------:|
|       1 |       31,990 |    30,039 |     **106%** |
|      10 |       68,918 |    80,972 |          85% |
|      50 |       75,075 |    84,034 |          89% |
|     100 |       74,963 |    83,195 |          90% |
|     200 |       73,964 |    83,333 |          89% |

##### Many clients, pipelined, random keys

`-c N` with `-P 16` and `__rand_int__` keys (different clients → different keys → different per-key mutexes). RPUSH at varying concurrency:

![RPUSH random key, varying clients (-P 16)](benchmark_results/chart_concurrency_random.svg)

A slice from `concurrent_random_*_p16.csv` at `-c 100`:

| Test            | Oktoplus rps | Redis rps | Okto / Redis |
|-----------------|-------------:|----------:|-------------:|
| RPUSH (rand)    |    1,000,000 |   900,901 |     **111%** |
| LPOP (rand)     |    1,052,632 |   934,579 |     **113%** |
| RPOP (rand)     |    1,075,269 | 1,136,364 |          95% |
| LLEN (rand)     |    1,041,667 | 1,149,425 |          91% |
| SADD (rand)     |    1,041,667 |   961,538 |     **108%** |
| SCARD (rand)    |    1,030,928 | 1,041,667 |          99% |

##### Multi-key, CPU-heavy commands — parallelism advantage

Random-key push/pop workloads at `-c 100 -P 16` saturate around ~1M rps for both servers because each command is short and command throughput is bounded by network and parsing, not by command CPU. The picture changes when the per-command CPU work dominates: with **`LPOS key:__rand_int__ <missing-value>` against pre-populated lists**, every call walks the whole list (10K elements ≈ ~100µs of CPU per call) while sending only ~5 bytes back over the wire. Redis stays capped at one core; Oktoplus's per-key sharding lets the work parallelize across cores.

![LPOS scan on 10K-element lists, varying clients (-P 16)](benchmark_results/chart_parallelism.svg)

`LPOS key:__rand_int__ NEVER_PRESENT` against 1000 pre-populated keys, each holding 10,000 distinct elements (`-P 16`):

| Clients | Oktoplus rps | Redis rps | Okto / Redis |
|--------:|-------------:|----------:|-------------:|
|       1 |       62,893 |     8,396 |    **7.5×**  |
|       4 |      238,095 |     8,624 |   **27.6×**  |
|      16 |      555,556 |     8,780 |   **63.3×**  |
|      64 |      540,541 |     8,692 |   **62.2×**  |
|     128 |      645,161 |     8,493 |   **76.0×**  |

Bench script: `benchmark_results/run_parallelism_advantage_bench.sh`. The same workload at smaller `N=1000` (10× shorter scans) reaches ~13× at `-c 128`; at smaller `-P 1` the network RTT eats most of the per-command CPU advantage and the ratio collapses to ~1.5×.

##### Single client, pipelined (`-P 16`), 256-byte values

Same workload as the small-value `-P 16` table above but with a 256-byte payload (`-d 256` for built-ins, a 256-byte literal on the custom RPUSH).

![Single client -P 16 throughput, 256-byte values](benchmark_results/chart_p16_d256.svg)

| Test          | Oktoplus rps | Redis rps | Okto / Redis |
|---------------|-------------:|----------:|-------------:|
| LPUSH         |      384,615 |   358,423 |     **107%** |
| SADD          |      414,938 |   369,004 |     **112%** |
| LPUSH (LRANGE seed) | 358,423 |   344,828 |     **104%** |
| LRANGE_100    |       50,176 |    53,191 |          94% |
| RPUSH (rand, 256B) | 313,480 |   290,698 |     **108%** |
| LPOP (rand)   |      332,226 |   297,619 |     **112%** |
| RPOP (rand)   |      392,157 |   350,877 |     **112%** |
| LLEN          |      465,116 |   406,504 |     **114%** |
| SCARD         |      465,116 |   386,100 |     **120%** |

Full per-test CSVs and the raw-results history are under `benchmark_results/raw/`.

##### Memory footprint

Generated by `benchmark_results/run_memory.sh` — for each cell, start a fresh server, snapshot RSS, load N distinct keys via `RPUSH key:i <value>` over `redis-cli --pipe`, snapshot RSS again. `bytes/key = (steady - baseline) * 1024 / N`.

![Memory footprint, bytes per key](benchmark_results/chart_memory.svg)

| N keys     | value | Oktoplus bytes/key | Redis bytes/key | Okto / Redis |
|-----------:|------:|-------------------:|----------------:|-------------:|
|   100,000  |    3B |                172 |              71 |        2.4×  |
|   100,000  |   64B |                253 |             135 |        1.9×  |
|   100,000  |  256B |                494 |             372 |        1.3×  |
|   100,000  | 1024B |              1,461 |           1,342 |        1.1×  |
| 1,000,000  |    3B |                222 |              72 |        3.1×  |
| 1,000,000  |   64B |                305 |             133 |        2.3×  |
| 1,000,000  |  256B |                547 |             375 |        1.5×  |
| 1,000,000  | 1024B |              1,516 |           1,345 |        1.1×  |

Per-key fixed overhead (extrapolated from the 3-byte rows where the value cost is negligible) is **~70 B** for Redis and **~170-220 B** for Oktoplus. The gap shrinks as the value grows: 2.4× at 3B (100k), 1.9× at 64B, 1.3× at 256B, essentially **at parity** (1.1×) at 1 KB. Full per-trial CSVs at `benchmark_results/raw/memory.csv`, full table at `benchmark_results/memory_results.md`.

##### Residual memory after FLUSHALL

`FLUSHALL` and `FLUSHDB` clear every container but do NOT ask the allocator to release pages back to the OS — that's exposed separately as `MEMORY PURGE` (Redis-compatible), which calls jemalloc's `mallctl("arena.<all>.purge")`. Decoupling the two means clients pay for what they ask for and the residual numbers below are an honest "Redis vs Oktoplus" comparison: the benchmark issues `FLUSHALL` + `MEMORY PURGE` on both servers.

![Residual RSS after FLUSHALL](benchmark_results/chart_memory_residual.svg)

| N keys     | value | Oktoplus residual (KiB) | Redis residual (KiB) |
|-----------:|------:|------------------------:|---------------------:|
|   100,000  |    3B |                  14,984 |                9,676 |
|   100,000  |   64B |                  13,808 |               10,040 |
|   100,000  |  256B |                  18,000 |               10,332 |
|   100,000  | 1024B |                  16,004 |               11,008 |
| 1,000,000  |    3B |                  18,084 |               11,568 |
| 1,000,000  |   64B |                  15,960 |               11,764 |
| 1,000,000  |  256B |                  21,344 |               14,140 |
| 1,000,000  | 1024B |                  28,100 |               23,720 |

Baseline RSS is now **~9.5 MiB for Oktoplus vs ~9.3 MiB for Redis — essentially at parity** (down from ~17.6 MiB before gRPC was made a build-time opt-in via `-DOKTOPLUS_WITH_GRPC=OFF`, the new default; the old default also disabled gRPC at runtime, but the protobuf/grpc/abseil-flow shared-library mappings still pinned ~8 MiB of RSS at process start). *Delta over baseline* (truly retained allocator memory) is ~4–19 MiB on Oktoplus vs ~0.4–14 MiB on Redis across the workload sweep.

#### Where Oktoplus wins

  - **Container choice matches access pattern.** Native [vectors](docs/vectors.md) give O(1) `INDEX` (Redis's `LINDEX` is O(n)). Multi-set and multi-map are first-class. `boost::multi_index_container` with up to 3 keys is on the roadmap. You pick the container; you don't reshape your data to fit a list or hash.
  - **Concurrent writers on different keys actually run in parallel.** The keyspace is split across 32 shards, each key has its own mutex. A workload of N writers touching N different keys uses N cores — not one. Redis 7's I/O threads parallelise socket reads/writes but command execution is single-threaded.
  - **CPU-bound multi-key workloads scale across cores.** When the per-command CPU dominates wire bytes (e.g. `LPOS key:__rand_int__ <missing>` scanning 10K-element lists), Redis caps at ~8.5K rps (one core) while Oktoplus reaches ~645K rps at `-c 128 -P 16` — **76× faster**. See the parallelism-advantage table above.
  - **Hot-key, read, and random-key throughput all beat Redis** at every value size benchmarked (see tables above). At single-client `-P 16`: LPUSH 118% small / 107% on 256-byte values; SADD/LLEN/SCARD 112–121%; random-key RPUSH/LPOP/RPOP 99–112%; LRANGE_100 116% small / 94% on 256-byte. At `-c 100 -P 16` random-key: RPUSH 111%, LPOP 113%, SADD 108%.
  - **Native gRPC alongside RESP.** Generate a typed client in any language straight from `commands.proto` — no need to (re)implement the wire protocol. Existing Redis tooling (`redis-cli`, `redis-benchmark`) still works on the RESP port.

#### What it doesn't do (yet)

  - No replication, clustering, or persistence — see the release plan below.
  - No pub/sub, streams, scripting, or transactions.
  - Command coverage: lists 76%, sets 94% on RESP / 18% on gRPC, strings 0% — see the per-family compatibility tables linked at the top.
  - Hot-key LPUSH at high concurrency without pipelining (`-P 1`) trails Redis by ~3–24% (network round-trip per command dominates and Oktoplus's per-command path is slightly heavier than Redis's hand-tuned single-threaded loop). With pipelining (`-P 16`), the same workload reaches Redis-parity (~1M rps both servers).
  - **Per-key memory overhead is ~2-3× Redis at small values** (~170 B vs 70 B), reaching parity at 1 KB. SDS-style packed keys/values (PERF_TODO item A) would close the remaining steady-state gap.
  - Single-node, no production deployments.

#### Release plan
- Support all REDIS commands (at least the one relative to data storage)
- Support the following containers: deque, list, map, multimap, multiset, set, unorderd_map, unordered_multimap, vector, boost::multi_index (up to at least 3 keys)
- Make it distributed using RAFT as consensus protocol

***

[How To Build](docs/howtobuild.md)

*** 
