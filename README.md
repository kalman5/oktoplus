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

Hardware: AMD EPYC Genoa devserver. Build: `-O3 -march=native -mtune=native -ffast-math -fno-semantic-interposition -funroll-loops`. Workload: 100k ops, 100k key-space, single client unless stated otherwise.

![Single client -P 16 throughput](benchmark_results/chart_p16.svg)

![LPUSH on a hot key, varying clients](benchmark_results/chart_concurrency.svg)

> Charts are generated from `benchmark_results/raw/*.csv` by `benchmark_results/make_chart.py` (no dependencies — pure-stdlib Python emitting SVG + HTML).
>
> An interactive Chart.js dashboard with the same data lives at [`benchmark_results/report.html`](benchmark_results/report.html) — view it rendered through [htmlpreview.github.io](https://htmlpreview.github.io/?https://github.com/kalman5/oktoplus/blob/master/benchmark_results/report.html).

##### Single client, no pipelining (`-P 1`) — both servers tied

At pipeline depth 1 the workload is dominated by the kernel network round-trip, not the server. Oktoplus and Redis are within run-to-run noise.

| Test          | Oktoplus rps | Redis rps | Okto / Redis |
|---------------|-------------:|----------:|-------------:|
| LPUSH         |       32,895 |    31,437 |         105% |
| SADD          |       30,321 |    30,722 |          99% |
| LRANGE_100    |       24,777 |    25,893 |          96% |
| LPOP (rand)   |       27,724 |    29,612 |          94% |
| RPOP (rand)   |       29,833 |    30,303 |          98% |
| LLEN (rand)   |       31,456 |    32,072 |          98% |
| SCARD (rand)  |       32,949 |    29,551 |         111% |

##### Single client, pipelined (`-P 16`) — Oktoplus 76–100% of Redis

Pipelining lets each server stretch its legs. Both servers are CPU-bound here; Redis's hand-tuned C still wins on the write paths but Oktoplus closes most of the gap, ties on `SCARD`, and `SADD` is now within 5%.

| Test          | Oktoplus rps | Redis rps | Okto / Redis |
|---------------|-------------:|----------:|-------------:|
| LPUSH         |      321,543 |   389,105 |          83% |
| SADD          |      319,489 |   334,448 |          96% |
| LRANGE_100    |       82,576 |   108,460 |          76% |
| LPOP (rand)   |      175,439 |   421,941 |          42% |
| RPOP (rand)   |      217,391 |   380,228 |          57% |
| LLEN (rand)   |      333,333 |   425,532 |          78% |
| SCARD (rand)  |      369,004 |   389,105 |          95% |

##### Many clients, no pipelining — LPUSH on a hot key

The "parallelism" sweep keeps `-P 1` and varies `-c`. Both servers saturate around 10 clients on a hot key (one TCP connection per client; everything serializes on the inner mutex / single-thread loop respectively).

| Clients | Oktoplus rps | Redis rps | Okto / Redis |
|--------:|-------------:|----------:|-------------:|
|       1 |       32,520 |    31,506 |         103% |
|      10 |       68,918 |   100,000 |          69% |
|      50 |       68,681 |    93,110 |          74% |
|     100 |       72,307 |    80,775 |          90% |
|     200 |       41,824 |    89,606 |          47% |

Full per-test CSVs and the raw-results history are under `benchmark_results/raw/`.

#### Release plan
- Support all REDIS commands (at least the one relative to data storage)
- Support the following containers: deque, list, map, multimap, multiset, set, unorderd_map, unordered_multimap, vector, boost::multi_index (up to at least 3 keys)
- Make it distributed using RAFT as consensus protocol

***

[How To Build](docs/howtobuild.md)

*** 
