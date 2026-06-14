# Number-encoding exploration: should Oktoplus intern numbers?

**Question explored:** adopt "a fixed vocabulary for numbers so we can just store the
number index instead" (Redis-inspired shared-integer objects), to shrink memory.

**Bottom line:** A general number vocabulary / inline-int encoding on
`okts::stor::string` is **not worth doing**. Small integers (≤15 digits — covers all
`int32` and `int64` up to 999,999,999,999,999) **already live inline in the fixed
16-byte SSO slot with zero heap allocation**, so there is no allocation to elide, and
the slot **cannot shrink** — it is pinned to exactly 16 B by `static_assert`
(`string.h:217-218`) and simultaneously *is* the shard-map key type
(`containerfunctorapplier.h:195-198`). The one integer technique whose memory win
survives adversarial review is **intset for Sets only** — the lone container still
using a 32-byte `std::string` per member.

## Why the vocabulary idea (store an index) doesn't transfer

Redis's shared-integer objects exist *only* because Redis allocates a
`redisObject + SDS` heap block per value; the shared singleton avoids that allocation.
Oktoplus has no such baseline:

- A 1–15 digit integer is already inline in the 16 B SSO slot, zero alloc
  (`string.h:231-236`). Nothing to eliminate.
- The slot can't shrink to benefit from a 2-byte index — it's fixed at 16 B and is also
  the map key. A 2-byte index just sits in a 16-byte hole, saving 0.
- It would *add*: a ~160 KB always-resident table, a hard range cap (inline-int has
  none), a never-free invariant, and a second indirection on every read/compare/hash.
- The heterogeneous-lookup hash contract forces recovering the decimal text and hashing
  *that* anyway — so even the indirection buys nothing.

It is strictly dominated by plain inline-int, which is itself not worth doing here.

## Approach scores (10 = best)

| Approach | Score | Why |
|---|--:|---|
| **C — intset for Sets** (sorted `vector<int64_t>` + upgrade-to-string fallback) | **7** | Only design whose memory win survives critique. Sets are the odd one out at 32 B/member; packing all-integer sets to 8 B int64 is a real ~3.5× density/RSS win. No key/hash-contract risk (changes the value container, not the key). |
| C — packed-int Lists (`devector<int64_t>`) | 2 | Ints already store zero-alloc inline in the 16 B list slot. Saves only 8 B density while *adding* a `to_chars` decode to every LRANGE/LINDEX/LPOP — regressing the exact scan path the ~10× per-core advantage rides on. |
| A — inline int64 in a still-16 B slot | 1 | Zero memory delta (slot stays 16 B, ints already inline). Net-negative CPU. int64 needs 20 digits of read scratch that doesn't fit 16 B; an out-of-line cache breaks the pure-memcpy move. |
| D — smaller tagged 8/16 B slot | 0 | Flat containers use one fixed stride (the 16 B string arm), so an int slot still costs 16 B → zero density. Shrinking below 16 B drops SSO and re-adds heap allocs for 7–15 byte strings (a regression on the non-numeric values the workload actually uses). |
| B — fixed vocabulary, store index (the literal idea) | 0 | Strictly dominated: 0 bytes saved, +160 KB table, hard range cap, double indirection. |

## Recommendation: intset for Sets only

Introduce a variant inside the Sets container storage: either a sorted
`std::vector<int64_t>` (INT mode) or the existing `flat_hash_set<std::string>`
(STRING mode), with a **one-way** upgrade.

- On SADD, `std::from_chars` each member. While in INT mode and every member round-trips
  **byte-identically** (re-render via `std::to_chars` + `memcmp` vs the original bytes —
  *not* merely "from_chars consumed all input"), binary-search-insert into the sorted
  vector.
- On the first non-canonical-integer member **or** on exceeding a size cap (mirror
  Redis's `set-max-intset-entries = 512` to bound the O(n) insert), materialize the
  string set once and never downgrade.
- Read paths (`forEachMember`/SMEMBERS, SINTER/SDIFF/SUNION, SPOP/SRANDMEMBER) gain an
  INT branch that `to_chars`-renders into a thread_local buffer consumed before the next
  iteration.

### Expected win
Sets only, all-integer sets only. Per-member **32 B std::string → 8 B int64**. Effective
open-addressed footprint ~37 B/member → ~9–10 B, i.e. **~3.5×** (not a clean 4× — the
sorted vector carries geometric capacity slack unless `shrink_to_fit`). A 512-member
integer set: ~19 KB → ~5 KB. The published 133–157 B/key table moves **0 bytes** — that
overhead is the map slot + ProtectedContainer + embedded `std::mutex` + container chain,
not member bytes.

### Smallest shippable slice
1. `sets.h:17-19` — replace the single `flat_hash_set<std::string>` Container with a
   wrapper holding the variant + mode tag + canonical-int helper.
2. `sets.cpp` — add INT branch + upgrade hook to `add()` and
   `isMember`/`cardinality`/`remove` first (independently testable: SADD/SISMEMBER/SREM
   correct in INT mode).
3. Defer diff/inter/union/*STORE/pop/randMember to a second commit — have them
   force-upgrade to STRING mode at entry so they keep working unchanged.
4. **Add an all-integer SADD memory cell** to the benchmark — current
   `run_memory.sh`/`run_benchmark.sh` store `'aaa'`/`'val'` values and put `__rand_int__`
   only in *key* names, so they'd show ZERO movement.

## Risks
- **Canonicalization corruption:** `from_chars` accepts `-`, parses `007`→7. Without
  re-render+memcmp, `SADD 007` then `SMEMBERS` silently returns `7`. `+7`, ` 7`, `7\n`,
  `-0`, and `>int64` must all fall back to STRING.
- **O(n) write cliff:** sorted-vector insert is O(n) memmove — must cap and upgrade or a
  1M-element int set regresses SADD badly.
- **Cross-mode *STORE:** an INT result stored into a STRING-mode destination forces an
  element-wise re-encode and can silently downgrade, eroding the win.
- **forEachMember buffer coupling:** INT mode `to_chars` into a reused buffer that
  SMEMBERS must copy before the next iteration — an undocumented invariant zero-copy
  reply work would violate.
- **Measurement blindness:** the whole published suite stores non-numeric values; the win
  is unobservable without a new integer workload.

## Open questions (decide before building)
1. **What fraction of real set workloads are all-integer?** If mostly non-numeric, even
   the intset win is academic.
2. **Is the per-key overhead (~133–157 B) the real prize?** No value/key encoding touches
   it — it's the map slot + ProtectedContainer + `std::mutex` + container chain. A smaller
   lock or pooled ProtectedContainers might beat any number-encoding work.
3. **Cheaper first move:** migrate Sets `std::string`→`okts::stor::string` (32→16 B) with
   *no* integer logic. Simpler, helps **all** sets (not just integer ones), captures half
   the density win with none of the canonicalization/O(n)/cross-mode risk. intset becomes
   a later layer on top.

## Status

- **2026-06-14 — Step 1 SHIPPED:** Sets migrated `std::string` → `okts::stor::string`
  (`sets.h`, `sets.cpp`; gRPC `add_values` in `commands_set.cpp` renders via string_view).
  Per-member object 32 B → 16 B for all sets. No integer/canonicalization logic, so member
  bytes are preserved exactly — `SADD 007` and `7` remain distinct members (matches Redis,
  which only canonicalizes inside an intset). Builds clean (debug + optimized); storage
  (73) + RESP (36) unit tests green; live RESP smoke test confirms integer & string sets.
- **Next (optional):** intset layer (sorted `vector<int64_t>` + upgrade) on top, 16 B → 8 B
  for all-integer sets — only if benchmarks show integer sets are a real workload.
4. int64-only first, or include int16/int32 width promotion (≈2× more on small-magnitude
   sets, but doubles the encoding surface)?

---
*Generated from a 4-phase multi-agent exploration (understand → design panel →
adversarial critique → synthesis), 2026-06-14.*
