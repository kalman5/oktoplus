#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace okts::stor {

// Stable identifier for a registered waiter. Used to cancel
// (timeout / disconnect) without depending on iterator stability.
using WaiterId = uint64_t;

// One client suspended on BLPOP / BRPOP / BLMPOP / BLMOVE / BRPOPLPUSH
// for a given list key.
//
// `onWake` is invoked exactly once -- either with the value the
// producer transferred (push path), or with std::nullopt to signal
// the registration was cancelled (timeout / disconnect).
//
// `wantsFront` is the side this waiter prefers when woken: front for
// BLPOP-family, back for BRPOP-family. The producer's notify loop
// pops from this end and hands the value to the waiter.
//
// The struct is intentionally small and storage-layer-only; it knows
// nothing about io_contexts, sockets, or timers. The caller (RESP
// handler) wraps the platform-specific wake into an std::function
// that posts onto the right thread before invocation.
struct BlockingWaiter {
  // Wake callback signature. The argument is the value the producer
  // transferred from the list, or std::nullopt for a cancellation
  // (timeout / disconnect).
  using OnWake = std::function<void(std::optional<std::string>)>;

  WaiterId id         = 0;
  OnWake   onWake;
  bool     wantsFront = true;
};

} // namespace okts::stor
