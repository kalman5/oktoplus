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
| Hot-key writes (LPUSH/SADD) | tied or slight win | 88–97% |
| LRANGE_100 | 84% | 70% |
| **Random-key writes (RPUSH-rand)** | **38%** | **36%** |
| LPOP / RPOP rand | 50–64% | 56–62% |
| Read-only (LLEN/SCARD) | 102–117% (we win) | 111–117% (we win) |

Two structural cost centres dominate what's left:

1. **Per-key overhead on random-key paths** — outer-map insert + per-key
   `ProtectedContainer` allocation (mutex `unique_ptr` heap alloc) +
   first list/set node alloc. Confirmed by the d=256 result: gap is
   the same as d=3, so it's per-*key*, not per-*value*.
2. **Per-value storage density on writes** — `std::string` overhead
   (~32 bytes object + heap alloc when off-SSO) vs Redis SDS (~5 bytes
   header + payload, single alloc).

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

## J. Async I/O RESP server (`boost::asio` worker pool)

Already detailed in the option-3 sketch earlier in the conversation.
Replaces thread-per-connection with `io_context` + worker pool.
Mostly helps multi-client workloads and removes the
use-after-free-on-shutdown latent bug Paladin flagged.

- **Expected lift**: 1.5–2× on c100/c200 random-key, modest on
  single client. Single-client `-P 16` doesn't have multiple
  connections to fan out, so the gain is from cheaper syscalls and
  batched writes — maybe 5–10%.
- **Effort**: medium-to-large (~300 LOC) but well-scoped.
- **Risk**: medium — async lifetime correctness.
- **Bonus**: closes the UAF-on-shutdown bug as a byproduct.

---

## Suggested order

(B, C, D, F, H done — flat_hash_map outer, sharding, embedded mutex,
multi-iteration harness, jemalloc all landed. RPUSH-rand c=100
climbed from 19% → 33% of Redis; reads scaled to 82-93%.)

1. **E** (PGO) — likely cheap win, validates the harness.
2. **A** (SDS) — substantial work, decide based on residual gap.
3. **J** (async server) — biggest remaining lever for the c=100
   write-throughput gap.
4. **I** (quicklist) — only if A landed and tiny-value workloads
   are a real target.
