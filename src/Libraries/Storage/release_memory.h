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
  // purge every arena's dirty pages back to the OS.
  std::uint64_t myEpoch = 0;
  std::size_t   myEpochSz = sizeof(myEpoch);
  sMallctl("epoch", &myEpoch, &myEpochSz, &myEpoch, sizeof(myEpoch));
  sMallctl("arena.4096.purge", nullptr, nullptr, nullptr, 0);
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
