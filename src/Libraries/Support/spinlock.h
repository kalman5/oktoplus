#pragma once

#include <atomic>
#include <mutex>

#include <boost/thread/lock_guard.hpp>

#include "Support/noncopyable.h"

namespace okts::sup {

class SpinLock
{
  friend class std::lock_guard<SpinLock>;
  friend class boost::lock_guard<SpinLock>;
  DISABLE_EVIL_CONSTRUCTOR(SpinLock);

 public:
  SpinLock() = default;

 private:
  void lock() {
    while (theLocked.test_and_set(std::memory_order_acquire)) {
      // Void body, active wait
    }
  }

  void unlock() {
    theLocked.clear(std::memory_order_release);
  }

  std::atomic_flag theLocked = ATOMIC_FLAG_INIT;
};

} // namespace okts::sup
