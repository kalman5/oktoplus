#pragma once

#include <dlfcn.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace okts::stor {

inline void releaseMemoryToOs() {
  using MallctlFn = int (*)(const char*, void*, std::size_t*, void*, std::size_t);
  static auto sMallctl =
      reinterpret_cast<MallctlFn>(dlsym(RTLD_DEFAULT, "mallctl"));
  if (!sMallctl) sMallctl = reinterpret_cast<MallctlFn>(dlsym(RTLD_DEFAULT, "je_mallctl"));
  if (!sMallctl) return;

  std::uint64_t myEpoch = 0;
  std::size_t myEpochSz = sizeof(myEpoch);
  sMallctl("epoch", &myEpoch, &myEpochSz, &myEpoch, sizeof(myEpoch));

  // Force decay to 0 for immediate return, flush tcache, then purge/decay.
  // Setting arenas.dirty_decay_ms to 0 ensures jemalloc doesn't wait 10s.
  {
    int64_t zero = 0;
    std::size_t zsz = sizeof(zero);
    sMallctl("arenas.dirty_decay_ms", nullptr, nullptr, &zero, zsz);
    sMallctl("arenas.muzzy_decay_ms", nullptr, nullptr, &zero, zsz);
  }

  sMallctl("thread.tcache.flush", nullptr, nullptr, nullptr, 0);
  sMallctl("arena.0.decay", nullptr, nullptr, nullptr, 0);
  sMallctl("arena.4096.decay", nullptr, nullptr, nullptr, 0);
  sMallctl("arena.0.purge", nullptr, nullptr, nullptr, 0);
  sMallctl("arena.4096.purge", nullptr, nullptr, nullptr, 0);

  unsigned myNarenas = 0;
  std::size_t myNarenasSz = sizeof(myNarenas);
  if (sMallctl("arenas.narenas", &myNarenas, &myNarenasSz, nullptr, 0) == 0) {
    char myName[64];
    for (unsigned i = 0; i < myNarenas; ++i) {
      snprintf(myName, sizeof(myName), "arena.%u.decay", i);
      sMallctl(myName, nullptr, nullptr, nullptr, 0);
      snprintf(myName, sizeof(myName), "arena.%u.purge", i);
      sMallctl(myName, nullptr, nullptr, nullptr, 0);
    }
    sMallctl("thread.tcache.flush", nullptr, nullptr, nullptr, 0);
    sMallctl("arena.4096.decay", nullptr, nullptr, nullptr, 0);
    sMallctl("arena.4096.purge", nullptr, nullptr, nullptr, 0);
  }
}

inline std::size_t queryStat(const char* name) {
  using MallctlFn = int (*)(const char*, void*, std::size_t*, void*, std::size_t);
  static auto sMallctl = reinterpret_cast<MallctlFn>(dlsym(RTLD_DEFAULT, "mallctl"));
  if (!sMallctl) sMallctl = reinterpret_cast<MallctlFn>(dlsym(RTLD_DEFAULT, "je_mallctl"));
  if (!sMallctl) return 0;
  std::uint64_t myEpoch = 0;
  std::size_t myEpochSz = sizeof(myEpoch);
  sMallctl("epoch", &myEpoch, &myEpochSz, &myEpoch, sizeof(myEpoch));
  std::size_t val = 0;
  std::size_t sz = sizeof(val);
  if (sMallctl(name, &val, &sz, nullptr, 0) != 0) return 0;
  return val;
}

inline std::size_t allocatedBytes() { return queryStat("stats.allocated"); }

inline std::string statsJson() {
  using MallctlFn = int (*)(const char*, void*, std::size_t*, void*, std::size_t);
  static auto sMallctl = reinterpret_cast<MallctlFn>(dlsym(RTLD_DEFAULT, "mallctl"));
  if (!sMallctl) sMallctl = reinterpret_cast<MallctlFn>(dlsym(RTLD_DEFAULT, "je_mallctl"));
  if (!sMallctl) return "{}";
  std::uint64_t myEpoch = 0;
  std::size_t myEpochSz = sizeof(myEpoch);
  sMallctl("epoch", &myEpoch, &myEpochSz, &myEpoch, sizeof(myEpoch));
  auto q = [&](const char* n) -> std::size_t {
    std::size_t v=0, sz=sizeof(v);
    sMallctl(n, &v, &sz, nullptr,0);
    return v;
  };
  char buf[1024];
  snprintf(buf, sizeof(buf),
    "{\"allocated\":%zu,\"active\":%zu,\"resident\":%zu,\"retained\":%zu,\"metadata\":%zu,\"dirty\":%zu,\"muzzy\":%zu}",
    q("stats.allocated"), q("stats.active"), q("stats.resident"), q("stats.retained"), q("stats.metadata"),
    q("stats.arenas.0.pdirty"), q("stats.arenas.0.pmuzzy"));
  return std::string(buf);
}

} // namespace okts::stor
