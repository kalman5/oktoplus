# oktoplus

![alt text](docs/octopus-free.png "Oktoplus")

###### What is oktoplus
Oktoplus is a in-memory data store K:V where V is a container: std::list, std::map, boost::multi_index_container, std::set, you name it. Doing so the client can choose the best container for his own access data pattern.

If this reminds you of REDIS then you are right, I was inspired by it, however:

 - Redis is not multithread
 - Redis offers only basic containers
 - For instance the Redis command LINDEX is O(n), so if you need to access a value with an index would be better to use a Vector style container
  - There is no analogue of multi-set in Redis

Redis Commands Compatibility

  - [LISTS](docs/compatibility_lists.md) 76% Completed
  - [SETS](docs/compatibility_sets.md) 18% Completed
  - [STRINGS](docs/compatibility_strings.md) 0% Completed

**Oktoplus** specific containers (already implemented, see specific documentation)

  - [DEQUES](docs/deques.md)
  - [VECTORS](docs/vectors.md)

#### Wire protocols

The server exposes the same data through two interfaces:

  - **gRPC** (default port `50051`) — see `src/Libraries/Commands/commands.proto`. Use it to generate a client in your favourite language. Includes admin RPCs `flushAll` / `flushDb` plus all the list / set / deque / vector commands.
  - **RESP** (default port `6379`, optional) — wire-compatible with Redis, so existing tooling like `redis-cli` and `redis-benchmark` works out of the box. Enabled by setting `service.resp_endpoint` in the JSON config.

Currently implemented RESP commands include `PING`, `QUIT`, `INFO`, `SELECT`, `CLIENT`, `COMMAND`, `FLUSHDB`, `FLUSHALL`, the list family (`LPUSH`/`RPUSH`/`LPUSHX`/`RPUSHX`/`LPOP`/`RPOP`/`LLEN`/`LINDEX`/`LINSERT`/`LRANGE`/`LREM`/`LSET`/`LTRIM`/`LMOVE`/`LPOS`/`LMPOP`), and the set family (`SADD`/`SCARD`/`SDIFF`/`SDIFFSTORE`/`SINTER`/`SINTERCARD`/`SINTERSTORE`/`SISMEMBER`/`SMISMEMBER`/`SMEMBERS`/`SMOVE`/`SPOP`/`SRANDMEMBER`/`SREM`/`SUNION`/`SUNIONSTORE`).

Server is multithread, two different clients working on different containers (type or name) have a minimal interaction. For example multiple clients performing a parallel batch insert on different keys can procede in parallel without blocking each other.

#### Benchmarks

A scripted comparison against Redis on the same machine lives at `benchmark_results/` (script: `benchmark_results/run_benchmark.sh`). It starts both servers itself, runs `redis-benchmark` at single-client `-P 1`/`-P 16` and at varying concurrency `-c 1..200`, and dumps CSVs into `benchmark_results/raw/`.

Latest single-client `-P 16` numbers on a devserver (100k ops, 100k key-space):

| Test         | Oktoplus rps | Redis rps | Okto / Redis |
|--------------|-------------:|----------:|-------------:|
| LPUSH        |      327,869 |   456,621 |          72% |
| SADD         |      284,091 |   364,964 |          78% |
| LRANGE_100   |       76,104 |   110,132 |          69% |
| LLEN         |      300,300 |   378,788 |          79% |
| SCARD        |      350,877 |   395,257 |          89% |

#### Release plan
- Support all REDIS commands (at least the one relative to data storage)
- Support the following containers: deque, list, map, multimap, multiset, set, unorderd_map, unordered_multimap, vector, boost::multi_index (up to at least 3 keys)
- Make it distributed using RAFT as consensus protocol

***

[How To Build](docs/howtobuild.md)

*** 
