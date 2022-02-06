#pragma once

#include "Support/noncopyable.h"

#include <chrono>

namespace okts::sup {

class Chrono
{
 public:
  using Duration    = std::chrono::duration<double>;
  using Clock       = std::chrono::steady_clock;
  using PointInTime = std::chrono::time_point<Clock>;

  explicit Chrono()
      : theStart(now())
      , theStop(theStart)
      , theIsRunning(true) {
  }

  inline void start() {
    theStart     = now();
    theIsRunning = true;
  }

  inline double stop() {
    if (theIsRunning) {
      theStop      = now();
      theIsRunning = false;
    }
    const Duration myDiff = theStop - theStart;
    return myDiff.count();
  }

  double lapse() const {
    if (theIsRunning) {
      const Duration myDiff = now() - theStart;
      return myDiff.count();
    }
    const Duration myDiff = theStop - theStart;
    return myDiff.count();
  }

  bool isRunning() const {
    return theIsRunning;
  }

 private:
  static PointInTime now() {
    return Clock::now();
  }

  PointInTime theStart;
  PointInTime theStop;
  bool        theIsRunning;
};

} // namespace okts::sup