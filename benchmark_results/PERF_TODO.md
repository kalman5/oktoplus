# Performance backlog

Ideas to test, ordered by **expected impact ÷ risk**. Each entry says what
to change, what to measure, and what we already know. Tick `[x]` when an
idea has been benched (link the commit) regardless of whether it landed.

Source of truth for "did this help?" is `benchmark_results/raw/*.csv`,
not intuition. Always re-run the full bench (small + large value) before
committing.

## Where the remaining gap lives (snapshot)

Single client, `-P 16`, current HEAD vs Redis on the same box:

| | small value | 256-byte value |
|--|---:|---:|
| Hot-key writes (LPUSH/SADD) | 108% / 103% (we win) | 116% / 112% (we win) |
| LRANGE_100 | **112% (we win)** | **103% (we win)** |
| Random-key writes (RPUSH-rand) | **106% (we win)** | **108% (we win)** |
| LPOP / RPOP rand | 101% / 109% (we win) | 99% / 114% (we win) |
| Read-only (LLEN/SCARD) | 112–118% (we win) | 107–110% (we win) |

At `-c 100 -P 16`, RPUSH-rand 112%, LPOP-rand 113%, SADD-rand 102%,
SCARD-rand 104% — random-key paths are also at or above Redis.

The previous gap on random-key paths was almost entirely caused by
two `LOG(INFO)` lines in `ContainerFunctorApplier` firing on every
new-key insert and every empty-key eviction. Downgrading both to
`VLOG(2)` lifted LPOP-rand from 62% → 113% of Redis at `-c 100`,
RPUSH-rand from 86% → 112%, and flipped d256 random-key writes from
~80% → 99–114%. (Landed alongside the J commit; see git log for the
exact change.)

Remaining structural cost centres (now in the noise on rps but still
real for memory):

1. **Per-key memory overhead** — outer-map slot + `ProtectedContainer`
   (now with embedded mutex) + first list/set node alloc. Doesn't
   move rps any more, but still ~170–200 B/key vs Redis's ~70 B/key
   at small values.
2. **Per-value storage density** — `std::string` overhead (~32 bytes
   object + heap alloc when off-SSO) vs Redis SDS (~5 bytes header
   + payload, single alloc). Drives the residual memory gap; rps
   impact bounded by the fact that we're already winning most
   commands.

---

## A. SDS-equivalent value type

Replace `std::string` inside `Sets`, `Lists`, etc. with a custom
length-prefixed byte buffer (single allocation, header packed for short
strings, NUL-terminated so existing C-string code still works).

- **Expected lift**: 5–15% on writes, more for very short values.
- **Effort**: large. Touches every place that stores or returns a
  member: `Sets::Container = absl::flat_hash_set<okts::Sds>` (custom
  hash + equal), `SequenceContainer` element type, `RespParser`
  output, gRPC bridge.
- **Risk**: medium-to-high. Lots of surface, but the type can be a
  drop-in replacement if we expose `data()` / `size()` /
  `string_view()` operators and a transparent hash.
- **Pre-work**: pick a header layout (steal Redis's `sdshdr5/8/16/32`
  variants), decide whether to add a slab allocator for the small
  headers (probably yes — otherwise per-element malloc dominates).
- **How to measure**: same bench, watch `LPUSH (hot)` and `SADD (hot)`
  on both small and 256-byte tables.

## B. [x] Embed per-key mutex inline (landed in 7edecbc)

`ProtectedContainer` heap-allocates a `boost::recursive_mutex`. On
random-key workloads we pay one alloc per *new* key. Embedding the
mutex inline removes that alloc but breaks the current "move mutex
out before erasing" eviction trick.

Two ways to make it work:

- **B1**: never erase entries from the outer map (let empty
  containers stick around). Tests that assert `hostedKeys() == 0`
  after popping all elements would need to change to "size() == 0".
- **B2**: keep eviction but use a 2-phase reclaim: take outer
  exclusive + inner, drop the entry into a "graveyard" vector, do
  the actual destroy in a periodic sweep. Adds a sweeper.

- **Expected lift**: 10–20% on RPUSH-rand (one fewer heap alloc per
  new key, smaller `ProtectedContainer`).
- **Effort**: medium for B1, large for B2.
- **Risk**: medium — semantics change visible via `hostedKeys()`.

## C. [x] Outer map: `flat_hash_map<string, unique_ptr<ProtectedContainer>>` (landed in 0857641)

Already analysed in the conversation. `node_hash_map` was a wash; the
conjecture is that flat-table key-comparison-in-slot beats the extra
indirection in node tables. With `unique_ptr` value, references stay
stable across rehash.

- **Expected lift**: 3–10% across the board.
- **Effort**: ~10 lines + CMake link.
- **Risk**: low.
- **Note**: depends on item B's decision (if B1 lands, the value
  could be embedded directly without `unique_ptr`).

## D. [x] Sharded outer (kShards=32, landed in 2ad1979)

Last try with `boost::mutex` per shard regressed single-client
hot-key by 5–19% due to cache locality (hash + 1/N chance of hitting
the same shard's cache line as the previous call). With
`boost::shared_mutex` for read paths we'd at least let parallel
read-only ops on different keys not block each other. Still costs a
shard-index per call.

- **Expected lift**: +10–20% on c100/c200 random-key (already
  measured). Hot-key single-client likely still regresses.
- **Effort**: small (already prototyped twice).
- **Risk**: medium — needs a way to make the cost optional, e.g.
  config flag `numShards = 1` default, opt-in for high-concurrency
  deployments.

## E. Profile-guided optimization (PGO)

Compile with `-fprofile-generate`, run the bench as the training
workload, recompile with `-fprofile-use`. Lets gcc pick branch
weights, inlining, and basic-block layout based on actual hot paths.

- **Expected lift**: 5–15% across the board.
- **Effort**: small (build-script change). Multi-iteration bench
  harness (next item) makes this measurable.
- **Risk**: very low.

## F. [x] Multi-iteration bench harness (landed)

`run_benchmark.sh` now repeats each redis-benchmark invocation
`ITERATIONS` times (env var, default 1, set to 5 for publication
runs) and pipes the rows through `bench_aggregate.py` which emits
the median row per test plus a stderr line tagged `[ok]` or `[WARN]`
when `max/min > 1.5×`.

The harness immediately paid off: the previously-published "RPUSH
random-key 36% of Redis" was a cold-iteration outlier — the median
across N=5 sits at **256K rps (71% of Redis)**. README and charts
refreshed accordingly.

It also functions as a smoke test: a server crash or memory bug
during one iteration produces a `WARN` row that's hard to miss in
the bench log.

## G. LTO with `-flto=auto` + thin bench harness

Tried once, results were too noisy to call. With (F) in place, retry
and check whether cross-TU inlining (RespHandler → Sets) actually
moves the needle.

- **Expected lift**: 5–15%, but evidence first.
- **Effort**: small.
- **Risk**: low.

## H. [x] Allocator: jemalloc (landed in 3c50fac, README refresh in c972d42)

Tried tcmalloc and jemalloc via `LD_PRELOAD`, then wired jemalloc
into the executable link line (`OKTOPLUS_WITH_JEMALLOC=ON` by
default in CMake). jemalloc won the head-to-head:

  LRANGE_100 d256:   38K  -> 52K  (+38%)
  LRANGE_100 small:  94K  -> 106K (+13%)
  LPUSH-warmup d256: 306K -> 347K (+13.5%)
  SCARD d256:        455K -> 515K (+13%)
  RPUSH-rand c100:   65K  -> 71K  (+9%)
  Most other paths:  flat or small positive

tcmalloc had a similar LRANGE win but slightly regressed LPUSH;
jemalloc was a more uniform improvement.

## I. List byte-packing (Redis-style quicklist)

The big one. Replace `std::deque<std::string>` with a deque of
length-prefixed byte buffers, packing many entries into each block.
For 3-byte values this drops per-entry footprint from 32 → ~5 bytes.
For 256-byte values the win is smaller but still real because each
buffer holds many entries with one allocation.

- **Expected lift**: large for tiny values (~30–50% on RPUSH/LPOP),
  small (~5–10%) for typical sizes. Memory footprint also drops
  significantly.
- **Effort**: very large. Custom iterators for LRANGE/LINDEX,
  splitting blocks for LSET/LREM/LINSERT-by-pivot.
- **Risk**: high. Rewrites the inner loop of every list command.
- **Probably not worth it** unless we commit to the SDS direction
  too — the two pair naturally.

## J. [x] Async I/O RESP server (`boost::asio` worker pool, landed in b212b80)

Replaced thread-per-connection with N `io_context`s, each driven by
exactly one worker thread (no strand overhead, no work-migration
cache thrash). Acceptor lives on slot[0]; accepted sockets are
pinned round-robin to a slot for life. Per-connection state
machine: `async_read_some` → drain `tryParseCommand` → batched
`async_write` → repeat. Connections tracked as
`shared_ptr<Connection>` with `weak_ptr` registration on the
server, closing the use-after-free-on-shutdown bug Paladin flagged.

Bench delta (vs the previous thread-per-connection HEAD,
ITERATIONS=5):

  -c100 p16 SCARD-rand: 98%  -> **103%** of Redis (first random-key win)
  -c100 p16 RPUSH-rand: 73%  -> 86%
  -c100 p16 LLEN-rand:  89%  -> 88% (within noise)
  -c100 p16 RPOP-rand:  82%  -> 89%
  -P 16 d256 LPUSH:     91%  -> **112%** of Redis (flipped to a win)
  -P 16 LRANGE_100:     110% -> **116%**
  -P 16 d256 LRANGE:    104% -> 102% (within noise)

Single-client `-P 1` lost ~3% on hot-key writes (one extra
io-loop hop per command); acceptable given the multi-client wins
and the correctness fix.

**Also closes Q** (pipeline drain): the async loop returns control
on `NeedMore` instead of re-entering blocking `readCommand`, while
batched `async_write` preserves the cross-segment batching that the
previous tryReadCommandFromBuffer attempt gave up.

## R. Close the remaining post-FLUSHALL residual gap

`FLUSHALL` / `FLUSHDB` clear every container then call
`mallctl("arena.<all>.purge")` (see `Storage/release_memory.h`).
Post-flush RSS lands close to baseline, but a constant ~3–5 MiB
*delta-over-baseline* gap to Redis remains across the workload
sweep:

  100k x    3B:  Okto 5.5 MiB delta vs Redis 0.7 MiB
  100k x 1024B:  Okto 6.3 MiB delta vs Redis 1.7 MiB
    1M x    3B:  Okto 8.3 MiB delta vs Redis 2.2 MiB
    1M x 1024B:  Okto 17  MiB delta vs Redis 14  MiB

`arena.<all>.purge` releases dirty extents but jemalloc can hold
"muzzy" extents (madvise(MADV_FREE) but still mapped) and per-arena
metadata that purge alone doesn't reclaim. Default `narenas` on
Linux is `4 * ncpus` so on a 16-core box that's 64 arenas, each
carrying its own bookkeeping.

Things to try, cheapest first:
  - `arena.<all>.decay` *before* `arena.<all>.purge` to push muzzy
    extents through to dirty before purging.
  - `MALLOC_CONF=dirty_decay_ms:0,muzzy_decay_ms:0` at startup so
    jemalloc returns pages aggressively without explicit purge.
    Costs perf on bursty workloads (no reuse buffer); needs a
    bench A/B.
  - `thread.tcache.flush` on every worker thread — tcache hangs
    onto small-object slabs across allocations.
  - Lower `narenas` (e.g. `narenas:8`) — trades multi-threaded
    alloc throughput for less per-arena bookkeeping. Bench needed.

- **Effort**: small for the mallctl variants, medium for tcache
  iteration over threads, small for `MALLOC_CONF` (one env var,
  but needs a startup-vs-CMake decision).
- **Risk**: low; FLUSHALL is admin-only, latency cost is
  acceptable. `MALLOC_CONF` aggressive decay does affect steady-
  state perf — measure before committing.
- **Why**: closes the visible "retains 5x more memory after flush"
  story without giving up the per-key throughput jemalloc buys.

## Q. [x] Pipeline drain no longer re-enters blocking read (closed by J, b212b80)

Subsumed by item J. The async per-connection state machine returns
control to the io loop on `NeedMore` instead of calling blocking
`readCommand`, so a slow / dribbling client that sends one full
command plus a partial next frame no longer stalls the reply to
the first command. Cross-segment batching is preserved by batched
`async_write`, so the d256 single-client `-P 16` regression that
killed the previous `tryReadCommandFromBuffer` attempt does not
recur (LPUSH-d256 actually flipped to a 112% win).

---

## Memory observability (not pure perf — needed to *measure* perf)

Today we have no first-class way to answer "how much memory does
Oktoplus use vs Redis for the same dataset?". Three concrete asks:

## K. `INFO memory` command

Redis returns a section with `used_memory` (allocator-tracked),
`used_memory_rss` (kernel RSS), `used_memory_peak`, fragmentation
ratio, allocator name. Implement the subset we can produce cheaply
on the RESP and gRPC sides:

  - `used_memory`: `je_mallctl("stats.allocated", ...)` (jemalloc) or
    `mallinfo2().uordblks` fallback.
  - `used_memory_rss`: read VmRSS from `/proc/self/status`.
  - `mem_allocator`: hard-coded "jemalloc-<version>" / "libc".

- **Effort**: small (~100 LOC + INFO command dispatch).
- **Risk**: low.
- **Why**: lets the bench harness compare memory parity vs Redis
  without scraping `/proc/<pid>` from outside.

## L. `MEMORY USAGE <key>` command

Redis returns the per-key byte cost (object header + value heap +
per-element overhead). For us the natural shape is:
`sizeof(ProtectedContainer)` + container's own overhead +
sum(element string capacity) + outer-map slot cost.

- **Effort**: medium — need a `byteSize()` method on each storage
  type and the dispatch to call it under the per-key lock.
- **Risk**: low; read-only by design.
- **Why**: enables debugging "which key is huge?" patterns and
  validates SDS / quicklist work (items A / I) by showing the
  per-key drop directly.

## M. jemalloc stats web dashboard

We don't have a web interface yet. Wire a tiny HTTP server (could
be `boost::beast` reusing the existing asio dependency, or a
stand-alone `httplib`-style header) that exposes:

  - `/stats/jemalloc` — `je_malloc_stats_print()` HTML or JSON.
  - `/stats/info` — same payload as the future INFO command.
  - `/stats/keys?prefix=...` — sampled MEMORY USAGE results.

- **Effort**: medium (~400 LOC with templates + server lifetime).
- **Risk**: low; admin-only port, off by default.
- **Why**: jemalloc's stats are rich (per-arena dirty pages,
  fragmentation, large/small allocation breakdowns) and currently
  invisible. A dashboard makes them actionable without recompiles.
- **Pre-work**: pick the HTTP layer (likely `boost::beast` to avoid
  pulling in another dep), settle on auth model (none / loopback only).

---

## Correctness follow-ups from code review

Items called out by code review that need real architectural work,
deferred here so they're tracked rather than silently skipped.

## N. Ordered two-key locking for cross-key LMOVE / RPOPLPUSH

`SequenceContainer::move()` cross-key path serialises every LMOVE
through the global `theMoveMutex` to avoid the L1<->L2 deadlock.
Acceptable today — LMOVE isn't a hot command — but defeats sharding
once it becomes one.

Replace with ordered two-key locking: at function entry, sort the
two keys (or their hashes) and acquire the inner mutexes in
deterministic order so two concurrent moves can't deadlock. Drop
`theMoveMutex` entirely.

- **Effort**: medium. Need a "lock two keys atomically" helper on
  `ContainerFunctorApplier` that takes inner mutexes in hash-sorted
  order. Same-key case already special-cased.
- **Risk**: medium — new locking protocol; needs a stress test for
  the deadlock case.
- **Why**: lets non-overlapping LMOVEs run in parallel; matches
  Redis-equivalent atomicity without a global serialisation point.

## O. Cross-shard snapshot semantics for Sets multi-key ops

`Sets::diff` / `Sets::inter` / `Sets::unionSets` walk the input keys
one by one, taking and releasing the per-key inner mutex
independently for each. A concurrent `SADD`/`SREM` on a later input
can therefore produce a result that was never simultaneously consistent
across all inputs. Real Redis is single-threaded and so doesn't have
this race.

Same root cause for `*STORE` variants (`SDIFFSTORE`,
`SINTERSTORE`, `SUNIONSTORE`): the snapshot is computed first, locks
released, then the destination is written. If the destination overlaps
the source set, or another writer races, the result is stale.

Two reasonable fixes:
  - Multi-shard ordered locking: pre-sort the involved (shard, key)
    pairs and lock all of them before reading. Atomic but coarse.
  - Optimistic snapshot + version check: compute, then re-verify
    each input wasn't mutated. Cheaper but more code.

- **Effort**: medium-large.
- **Risk**: medium — locking new code paths.
- **Why**: matches Redis's atomicity guarantees.

## P. [x] Drop `std::list<std::string>` return type from list ops (landed)

`SequenceContainer::popFront` / `popBack` / `range` / `multiplePop`
returned `std::list<std::string>` — one heap-alloc per element.
Switched to `std::vector<std::string>` with an `if constexpr (requires
{ c.size(); })` reserve in pop, and an exact `reserve(stop-start+1)`
in range.

Bench delta (single client `-P 16`, ITERATIONS=5):

  LRANGE_100 small:  108K -> 123K rps (+13.7%)  -> 110% of Redis
  LRANGE_100 d256:    53K ->  56K rps  (+5.3%)  -> 104% of Redis
  LPOP/RPOP rand:    within noise (bench pops 1 element/call, so
                     reserve(1) is essentially free either way)

LRANGE_100 went from below parity (84% small / 70% d256) to a win.

---

## Suggested order

(B, C, D, F, H, J, P, Q done — flat_hash_map outer, sharding, embedded
mutex, multi-iteration harness, jemalloc, async asio server, vector-
return on pop/range, async pipeline-drain correctness all landed.
After J, RPUSH-rand c=100 reached 86% of Redis, SCARD-rand became
the first random-key win at 103%, and LPUSH-d256 flipped to a 112%
win at single-client `-P 16`.)

1. **K** (`INFO memory`) — unblocks honest memory comparison vs
   Redis in the bench harness.
2. **R** (post-FLUSHALL residual) — small mallctl tweaks to close
   the last ~3–5 MiB delta over baseline. Cheap, contained.
3. **E** (PGO) — likely cheap win, validates the harness.
4. **A** (SDS) — substantial work; the largest remaining lever now
   that the I/O bottleneck is gone. Decide based on steady-state
   per-key gap (~2–3× Redis at small values).
5. **L** (`MEMORY USAGE <key>`) — pairs with A to *show* the
   per-key drop.
6. **M** (jemalloc web dashboard) — admin/observability sugar; do
   after K so the data plumbing already exists.
7. **I** (quicklist) — only if A landed and tiny-value workloads
   are a real target.
