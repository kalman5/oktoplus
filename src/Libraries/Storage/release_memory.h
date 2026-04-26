#pragma once

#include <dlfcn.h>
#include <cstddef>

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
  sMallctl("arena.4096.purge", nullptr, nullptr, nullptr, 0);
}

} // namespace okts::stor
