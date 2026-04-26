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

// Bare TTAS spinlock with no scheduler yield. For critical sections
// that are guaranteed short (microseconds at most) and where
// contended `sched_yield` calls would dominate cost.
//
// Two reasons this beats SpinLock under heavy contention:
//
//  1. No yield: the standard SpinLock yields after 64 spins as a
//     safety net against long-held locks. That yield is a syscall
//     (~100s of ns) and contends on kernel scheduler internals;
//     when 50 threads hammer one hot key the yields overwhelm the
//     lock-holder's progress and throttle throughput.
//
//  2. Test-and-test-and-set: the inner spin uses a relaxed *load*
//     and only attempts the (expensive) RMW exchange when the lock
//     looks free. Plain test_and_set in a loop dirties the cache
//     line on every iteration — every other core then has to
//     invalidate and re-fetch, which under 50-way contention
//     halves throughput vs the TTAS pattern.
//
// Use SpinLock when the holder might be slow or block; use
// BareSpinlock only when the critical section is provably short.
class BareSpinlock
{
  friend class std::lock_guard<BareSpinlock>;
  friend class std::unique_lock<BareSpinlock>;
  DISABLE_EVIL_CONSTRUCTOR(BareSpinlock);

 public:
  BareSpinlock() = default;

 private:
  void lock() {
    while (true) {
      if (!theFlag.exchange(true, std::memory_order_acquire)) {
        return;
      }
      // Relaxed load until the lock looks free — keeps the cache
      // line in shared state on every spinning core, so only the
      // holder's release dirties it.
      while (theFlag.load(std::memory_order_relaxed)) {
        OKTOPLUS_CPU_RELAX();
      }
    }
  }

  void unlock() {
    theFlag.store(false, std::memory_order_release);
  }

  std::atomic<bool> theFlag{false};
};

} // namespace okts::sup

#undef OKTOPLUS_CPU_RELAX
