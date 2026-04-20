#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "Support/noncopyable.h"

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#define OKTOPLUS_CPU_RELAX() _mm_pause()
#elif defined(__aarch64__)
#define OKTOPLUS_CPU_RELAX() asm volatile("yield" ::: "memory")
#else
#define OKTOPLUS_CPU_RELAX() ((void)0)
#endif

namespace okts::sup {

class SpinLock
{
  friend class std::lock_guard<SpinLock>;
  DISABLE_EVIL_CONSTRUCTOR(SpinLock);

 public:
  SpinLock() = default;

 private:
  void lock() {
    // Spin briefly with a CPU pause hint (avoids hammering the cache
    // line and starving an SMT sibling), then yield to the scheduler
    // if the lock is still held — long-held locks shouldn't be
    // burning a core, and yielding lets the holder make progress.
    constexpr int kSpinBeforeYield = 64;
    for (int i = 0; theLocked.test_and_set(std::memory_order_acquire); ++i) {
      if (i < kSpinBeforeYield) {
        OKTOPLUS_CPU_RELAX();
      } else {
        std::this_thread::yield();
        i = 0;
      }
    }
  }

  void unlock() {
    theLocked.clear(std::memory_order_release);
  }

  std::atomic_flag theLocked = ATOMIC_FLAG_INIT;
};

} // namespace okts::sup

#undef OKTOPLUS_CPU_RELAX
