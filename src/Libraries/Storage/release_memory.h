#pragma once

#include <dlfcn.h>
#include <cstddef>
#include <cstdint>

namespace okts::stor {

// Ask jemalloc to return dirty pages to the OS. Container clear()
// drops the contents but jemalloc holds the freed extents as dirty
// pages for reuse, so RSS stays high after FLUSHALL even though the
// heap is empty -- which made our post-FLUSHALL residual look ~10x
// worse than Redis's. Call this after clearing every container in
// FLUSHDB / FLUSHALL paths.
//
// Looked up via dlsym(RTLD_DEFAULT) so this is a no-op when jemalloc
// isn't linked (glibc has no mallctl symbol). 4096 ==
// MALLCTL_ARENAS_ALL in jemalloc -- the magic arena index meaning
// "every arena".
inline void releaseMemoryToOs() {
  using MallctlFn = int (*)(const char*, void*, std::size_t*, void*, std::size_t);
  static auto sMallctl =
      reinterpret_cast<MallctlFn>(dlsym(RTLD_DEFAULT, "mallctl"));
  if (sMallctl == nullptr) {
    return;
  }
  // Tell jemalloc to refresh its cached stats counters (so a follow-up
  // `stats.allocated` query reflects the post-purge reality), then
  // flush this thread's tcache and purge every arena's dirty pages back to the OS.
  // Flushing tcache is critical: after FLUSHALL the clearing thread's tcache
  // holds up to 100k+ freed extents (e.g. 1024B values). Without a flush,
  // arena.4096.purge sees no dirty pages in the arena itself and RSS stays high.
  std::uint64_t myEpoch = 0;
  std::size_t   myEpochSz = sizeof(myEpoch);
  sMallctl("epoch", &myEpoch, &myEpochSz, &myEpoch, sizeof(myEpoch));
  sMallctl("thread.tcache.flush", nullptr, nullptr, nullptr, 0);
  sMallctl("arena.4096.purge", nullptr, nullptr, nullptr, 0);
  // Also try explicit per-arena purge via narenas, for jemalloc versions
  // where 4096 magic is not enough, plus a second tcache flush after purge
  // to catch any allocations that were promoted.
  unsigned myNarenas = 0;
  std::size_t myNarenasSz = sizeof(myNarenas);
  if (sMallctl("arenas.narenas", &myNarenas, &myNarenasSz, nullptr, 0) == 0) {
    char myName[64];
    // Purge each arena individually (0..narenas-1) - some jemalloc builds
    // require explicit per-arena purge.
    for (unsigned i = 0; i < myNarenas; ++i) {
      snprintf(myName, sizeof(myName), "arena.%u.purge", i);
      sMallctl(myName, nullptr, nullptr, nullptr, 0);
    }
    // Final tcache flush + all-arenas purge to clean up anything
    // that was flushed from tcache into arena as dirty.
    sMallctl("thread.tcache.flush", nullptr, nullptr, nullptr, 0);
    sMallctl("arena.4096.purge", nullptr, nullptr, nullptr, 0);
  }
}

// Returns the number of bytes jemalloc currently believes are
// allocated (`stats.allocated`) — i.e. live application allocations
// not yet free()'d. Returns 0 when jemalloc isn't linked. Useful for
// leak hunting: stable across "load + flush + purge" cycles iff the
// application doesn't leak.
inline std::size_t allocatedBytes() {
  using MallctlFn = int (*)(const char*, void*, std::size_t*, void*, std::size_t);
  static auto sMallctl =
      reinterpret_cast<MallctlFn>(dlsym(RTLD_DEFAULT, "mallctl"));
  if (sMallctl == nullptr) {
    return 0;
  }
  // Refresh stats first; jemalloc caches them between epoch bumps.
  std::uint64_t myEpoch = 0;
  std::size_t   myEpochSz = sizeof(myEpoch);
  sMallctl("epoch", &myEpoch, &myEpochSz, &myEpoch, sizeof(myEpoch));

  std::size_t myAllocated = 0;
  std::size_t mySz        = sizeof(myAllocated);
  if (sMallctl("stats.allocated", &myAllocated, &mySz, nullptr, 0) != 0) {
    return 0;
  }
  return myAllocated;
}

} // namespace okts::stor
