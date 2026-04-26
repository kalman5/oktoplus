#pragma once

#include "Storage/genericcontainer.h"

#include "Support/noncopyable.h"

#include <boost/container/devector.hpp>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <limits>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace okts::stor::detail {
// Safe absolute value of an int64_t into uint64_t. std::abs(INT64_MIN)
// is undefined behaviour because the positive value isn't
// representable in int64_t; this helper returns the correct
// 9223372036854775808 in the unsigned domain.
inline uint64_t safeAbs(int64_t aValue) {
  if (aValue >= 0) {
    return static_cast<uint64_t>(aValue);
  }
  // -INT64_MIN as int64_t is UB; do the negation in unsigned where
  // wrap-around is well-defined and gives the correct magnitude.
  return static_cast<uint64_t>(0) - static_cast<uint64_t>(aValue);
}
} // namespace okts::stor::detail

namespace okts::stor {

template <class CONTAINER>
class SequenceContainer : public GenericContainer<CONTAINER>
{
  using Container = CONTAINER;
  using Base      = GenericContainer<Container>;

 public:
  using Status    = typename Base::Status;
  using Position  = typename Base::Position;
  using Direction = typename Base::Direction;

  DISABLE_EVIL_CONSTRUCTOR(SequenceContainer);

  SequenceContainer()
      : Base() {
  }

  size_t pushFront(const std::string&                   aName,
                   const std::vector<std::string_view>& aValues);

  std::vector<std::string> popFront(const std::string& aName, uint64_t aCount);

  size_t pushFrontExist(const std::string&                   aName,
                        const std::vector<std::string_view>& aValues);

  ////////////

  size_t pushBack(const std::string&                   aName,
                  const std::vector<std::string_view>& aValues);

  std::vector<std::string> popBack(const std::string& aName, uint64_t aCount);

  size_t size(const std::string& aName) const;

  std::optional<std::string> index(const std::string& aName,
                                   int64_t            aIndex) const;

  std::optional<int64_t> insert(const std::string& aName,
                                Position           aPosition,
                                const std::string& aPivot,
                                const std::string& aValue);

  std::optional<std::string> move(const std::string& aSourceName,
                                  const std::string& aDestinationName,
                                  Direction          aSourceDirection,
                                  Direction          aDestinationDirection);

  std::list<uint64_t> position(const std::string& aName,
                               const std::string& aValue,
                               int64_t            aRank,
                               uint64_t           aCount,
                               uint64_t           aMaxLength);

  std::vector<std::string>
  range(const std::string& aName, int64_t aStart, int64_t aStop) const;

  size_t
  remove(const std::string& aName, int64_t aCount, const std::string& aValue);

  Status
  set(const std::string& aName, int64_t aIndex, const std::string& aValue);

  void trim(const std::string& aName, int64_t aStart, int64_t aStop);

  size_t pushBackExist(const std::string&                   aName,
                       const std::vector<std::string_view>& aValues);

  std::vector<std::string>
  multiplePop(const std::vector<std::string>& aNames,
              Direction                       aDirection,
              uint64_t                        aCount);

  // ---- Blocking pop (BLPOP / BRPOP) -------------------------------------
  //
  // Single-key blocking pop. Tries the non-blocking pop first (under
  // the per-key lock). If the list has elements, returns the popped
  // value immediately and `*aWaiterId` is left at 0 — no waiter
  // registered, no cancellation needed.
  //
  // If the list is empty (or the key doesn't exist), registers
  // `aOnWake` as a waiter on the key, sets `*aWaiterId` to the new
  // waiter's id, and returns std::nullopt. The caller MUST track the
  // id and call cancelWaiter() if their timeout fires before the
  // producer wakes the waiter.
  std::optional<std::string>
  tryPopFrontOrWait(const std::string&  aName,
                    BlockingWaiter::OnWake aOnWake,
                    okts::stor::WaiterId*  aWaiterId);

  std::optional<std::string>
  tryPopBackOrWait(const std::string&     aName,
                   BlockingWaiter::OnWake aOnWake,
                   okts::stor::WaiterId*  aWaiterId);

  // Cancel a previously registered waiter (timeout / disconnect).
  // Returns true if the caller still owns the wake side (must invoke
  // its own onWake with std::nullopt), false if the producer beat us
  // to it (waiter already woken — onWake already invoked).
  bool cancelWaiter(const std::string& aName, okts::stor::WaiterId aId) {
    return Base::theApplyer.cancelWaiter(aName, aId);
  }

 private:
  using MoveMutex = std::mutex;
  // Serialises every cross-key move() to avoid the L1<->L2 deadlock
  // (see comment in move()'s cross-key branch). Held only by move();
  // no other code path acquires it.
  MoveMutex theMoveMutex;
};

// Lists were originally backed by std::list (one heap-alloc per push
// for the node), then std::deque (amortises allocs across blocks and
// has random-access iterators for LRANGE/LINDEX). The deque variant
// pays a 512-byte per-block pre-allocation in libstdc++ even for a
// single-element list; boost::container::devector is contiguous,
// supports O(1) push_front/push_back, and starts with zero capacity
// (allocation only on first push), so a 1-element list pays for
// exactly one std::string slot.
//
// NOTE: `Lists` and `Deques` resolve to the same C++ type today (both
// are SequenceContainer<devector<string>>). They are kept as separate
// using-aliases because StorageContext stores them in distinct
// instances — they do NOT share a keyspace. A `LPUSH foo v` populates
// `theStorage.lists`, a `DequePushBack foo v` populates
// `theStorage.deques`. The names denote the *namespace*, not a
// distinct backing implementation.
using Lists   = SequenceContainer<boost::container::devector<std::string>>;
using Deques  = SequenceContainer<boost::container::devector<std::string>>;
using Vectors = SequenceContainer<std::vector<std::string>>;

//// INLINE DEFINITIONS

template <class CONTAINER>
size_t SequenceContainer<CONTAINER>::pushFront(
    const std::string& aName, const std::vector<std::string_view>& aValues) {

  size_t myRet = 0;

  Base::theApplyer.performAndDrainWaiters(
      aName,
      [&aValues](Container& aContainer) {
        for (const auto& myString : aValues) {
          aContainer.emplace_front(myString);
        }
      },
      // Drain helpers: pop preferred end, return value (or nullopt
      // if container is now empty). Called once per waiter.
      [](Container& aContainer) -> std::optional<std::string> {
        if (aContainer.empty()) return std::nullopt;
        if constexpr (requires { aContainer.pop_front(); }) {
          std::string myValue = std::move(aContainer.front());
          aContainer.pop_front();
          return myValue;
        } else {
          // Containers without pop_front (e.g. std::vector) never
          // host BLPOP-style waiters anyway; return nullopt so the
          // drain loop is a no-op.
          return std::nullopt;
        }
      },
      [](Container& aContainer) -> std::optional<std::string> {
        if (aContainer.empty()) return std::nullopt;
        std::string myValue = std::move(aContainer.back());
        aContainer.pop_back();
        return myValue;
      },
      [&myRet](const Container& aContainer) {
        myRet = aContainer.size();
      });

  return myRet;
}

template <class CONTAINER>
std::vector<std::string>
SequenceContainer<CONTAINER>::popFront(const std::string& aName,
                                       const uint64_t     aCount) {

  std::vector<std::string> myRet;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, &aCount](Container& aContainer) {
        // Single allocation when the backing container exposes size():
        // we know an upper bound on how many elements we'll move out.
        // Keep the if constexpr guard so this template stays open to
        // future containers that might not be sized.
        if constexpr (requires { aContainer.size(); }) {
          myRet.reserve(std::min<uint64_t>(aCount, aContainer.size()));
        }
        uint64_t myCollected = 0;
        while (!aContainer.empty() && myCollected < aCount) {
          myRet.emplace_back(std::move(aContainer.front()));
          aContainer.pop_front();
          ++myCollected;
        }
      });

  return myRet;
}

template <class CONTAINER>
size_t SequenceContainer<CONTAINER>::pushFrontExist(
    const std::string& aName, const std::vector<std::string_view>& aValues) {

  size_t myRet = 0;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, &aValues](Container& aContainer) {
        for (const auto& myString : aValues) {
          aContainer.emplace_front(myString);
        }
        myRet = aContainer.size();
      });

  return myRet;
}

template <class CONTAINER>
size_t SequenceContainer<CONTAINER>::pushBack(
    const std::string& aName, const std::vector<std::string_view>& aValues) {

  size_t myRet = 0;

  Base::theApplyer.performAndDrainWaiters(
      aName,
      [&aValues](Container& aContainer) {
        for (const auto& myString : aValues) {
          aContainer.emplace_back(myString);
        }
      },
      [](Container& aContainer) -> std::optional<std::string> {
        if (aContainer.empty()) return std::nullopt;
        if constexpr (requires { aContainer.pop_front(); }) {
          std::string myValue = std::move(aContainer.front());
          aContainer.pop_front();
          return myValue;
        } else {
          // Containers without pop_front (e.g. std::vector) never
          // host BLPOP-style waiters anyway; return nullopt so the
          // drain loop is a no-op.
          return std::nullopt;
        }
      },
      [](Container& aContainer) -> std::optional<std::string> {
        if (aContainer.empty()) return std::nullopt;
        std::string myValue = std::move(aContainer.back());
        aContainer.pop_back();
        return myValue;
      },
      [&myRet](const Container& aContainer) {
        myRet = aContainer.size();
      });

  return myRet;
}

template <class CONTAINER>
std::vector<std::string>
SequenceContainer<CONTAINER>::popBack(const std::string& aName,
                                      const uint64_t     aCount) {

  std::vector<std::string> myRet;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, &aCount](Container& aContainer) {
        if constexpr (requires { aContainer.size(); }) {
          myRet.reserve(std::min<uint64_t>(aCount, aContainer.size()));
        }
        uint64_t myCollected = 0;
        while (!aContainer.empty() && myCollected < aCount) {
          myRet.emplace_back(std::move(aContainer.back()));
          aContainer.pop_back();
          ++myCollected;
        }
      });

  return myRet;
}

template <class CONTAINER>
size_t SequenceContainer<CONTAINER>::size(const std::string& aName) const {

  size_t myRet = 0;

  Base::theApplyer.performOnExisting(
      aName,
      [&myRet](const Container& aContainer) { myRet = aContainer.size(); });

  return myRet;
}

template <class CONTAINER>
std::optional<std::string>
SequenceContainer<CONTAINER>::index(const std::string& aName,
                                    int64_t            aIndex) const {

  std::optional<std::string> myRet;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, aIndex](const Container& aContainer) {
        if (aIndex >= 0) {
          if (size_t(aIndex) < aContainer.size()) {
            auto myIt = aContainer.begin();
            std::advance(myIt, aIndex);
            myRet = *myIt;
          }
        } else {
          // detail::safeAbs over int64 is defined for every input
          // including INT64_MIN; std::abs(INT64_MIN) is UB.
          auto myReverseIndex = static_cast<size_t>(detail::safeAbs(aIndex + 1));
          if (myReverseIndex < aContainer.size()) {
            auto myIt = aContainer.rbegin();
            std::advance(myIt, myReverseIndex);
            myRet = *myIt;
          }
        }
      });

  return myRet;
}

template <class CONTAINER>
std::optional<int64_t>
SequenceContainer<CONTAINER>::insert(const std::string& aName,
                                     Position           aPosition,
                                     const std::string& aPivot,
                                     const std::string& aValue) {
  std::optional<int64_t> myRet;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, aPosition, &aPivot, &aValue](Container& aContainer) {
        auto myIt = std::find(aContainer.begin(), aContainer.end(), aPivot);
        if (myIt == aContainer.end()) {
          myRet = -1;
          return;
        }

        if (aPosition == Position::AFTER) {
          ++myIt;
        }

        aContainer.insert(myIt, aValue);
        myRet = aContainer.size();
      });

  return myRet;
}

template <class CONTAINER>
std::optional<std::string>
SequenceContainer<CONTAINER>::move(const std::string& aSourceName,
                                   const std::string& aDestinationName,
                                   Direction          aSourceDirection,
                                   Direction          aDestinationDirection) {

  std::optional<std::string> myRet;

  // Same-key rotation (LMOVE k k LEFT RIGHT, RPOPLPUSH k k): handle
  // entirely under a single inner-lock acquisition to avoid re-entering
  // the same per-key mutex through the nested performOnNew below. This
  // is the only structural reason the per-key mutex used to be
  // recursive; with this special case it can be a plain std::mutex.
  if (aSourceName == aDestinationName) {
    Base::theApplyer.performOnExisting(
        aSourceName,
        [&aSourceDirection, &aDestinationDirection, &myRet](
            Container& aContainer) {
          if (aContainer.empty()) {
            return;
          }
          std::string myValue;

          if (aSourceDirection == Direction::LEFT) {
            myValue = aContainer.front();
            aContainer.pop_front();
          } else {
            myValue = aContainer.back();
            aContainer.pop_back();
          }

          if (aDestinationDirection == Direction::LEFT) {
            aContainer.push_front(myValue);
          } else {
            aContainer.push_back(myValue);
          }

          myRet = std::move(myValue);
        });
    return myRet;
  }

  // Cross-key move. theMoveMutex serialises all such moves to avoid
  // the classic deadlock between concurrent move(L1 -> L2) and
  // move(L2 -> L1) — both would hold one inner lock and spin waiting
  // for the other under the try-lock-retry protocol.
  //
  // The nested performOnNew(aDestinationName, ...) below runs INSIDE
  // the source's per-key lambda, so:
  //   - the source's per-key inner mutex is held throughout, AND
  //   - the destination's shard outer mutex is reacquired (different
  //     shard or same — either way it's a fresh acquisition, not
  //     re-entry of an already-held lock), AND
  //   - the destination's per-key inner mutex is acquired via the
  //     try-lock-retry loop. Safe because src and dst are guaranteed
  //     distinct keys here (the same-key case branched above), so the
  //     two inner mutexes are different ProtectedContainer instances.
  // theMoveMutex is therefore not the *only* serialisation in play —
  // the destination shard outer mutex is also taken — but it is what
  // guarantees no two cross-key moves can interleave their inner-lock
  // acquisitions and deadlock.
  // TODO: replace with ordered two-key locking so non-overlapping
  //       moves can proceed in parallel.
  const std::lock_guard myLock(theMoveMutex);

  Base::theApplyer.performOnExisting(
      aSourceName,
      [this,
       &aDestinationName,
       &aSourceDirection,
       &aDestinationDirection,
       &myRet](Container& aSourceContainer) {
        if (aSourceContainer.empty()) {
          return;
        }
        std::string myValue;

        if (aSourceDirection == Direction::LEFT) {
          myValue = aSourceContainer.front();
          aSourceContainer.pop_front();
        } else {
          myValue = aSourceContainer.back();
          aSourceContainer.pop_back();
        }

        Base::theApplyer.performOnNew(
            aDestinationName,
            [&myValue,
             &aDestinationDirection](Container& aDestinationContainer) {
              if (aDestinationDirection == Direction::LEFT) {
                aDestinationContainer.push_front(myValue);
              } else {
                aDestinationContainer.push_back(myValue);
              }
            });

        myRet = std::move(myValue);
      });

  return myRet;
}

template <class CONTAINER>
std::list<uint64_t>
SequenceContainer<CONTAINER>::position(const std::string& aName,
                                       const std::string& aValue,
                                       const int64_t      aRank,
                                       const uint64_t     aCount,
                                       const uint64_t     aMaxLength) {
  std::list<uint64_t> myRet;

  Base::theApplyer.performOnExisting(
      aName,
      [&myRet, &aValue, &aRank, &aCount, &aMaxLength](
          const Container& aContainer) {
        uint64_t myIndex = 0;
        uint64_t myFound = 0;

        const uint64_t myURank = detail::safeAbs(aRank);

        auto mySearchFunction = [&](const auto aBegin,
                                    const auto aEnd,
                                    const auto aIndexConverter) {
          for (auto myIt = aBegin; myIt != aEnd; ++myIt) {
            if (*myIt == aValue) {
              ++myFound;

              /// Until we have not found the amount of ranked values
              /// we need to skip the found ones.
              if (myFound < myURank) {
                ++myIndex;
                continue;
              }

              myRet.push_back(aIndexConverter(myIndex));
              if (myRet.size() == aCount) {
                break;
              }
            }

            ++myIndex;

            /// Interrupt the traverse if I have visited already the max
            /// allowed values.
            /// If specified length is 0 then we do not have limits.
            if (aMaxLength > 0 && myIndex >= aMaxLength) {
              break;
            }
          }
        };

        if (aRank > 0) {
          mySearchFunction(aContainer.begin(),
                           aContainer.end(),
                           [](uint64_t aIndex) { return aIndex; });
        } else {
          mySearchFunction(aContainer.rbegin(),
                           aContainer.rend(),
                           [myLen = aContainer.size()](uint64_t aIndex) {
                             return myLen - 1 - aIndex;
                           });
        }
      });

  return myRet;
}

template <class CONTAINER>
std::vector<std::string> SequenceContainer<CONTAINER>::range(
    const std::string& aName, int64_t aStart, int64_t aStop) const {

  std::vector<std::string> myRet;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, aStart, aStop](const Container& aContainer) {
        if (aContainer.empty()) {
          return;
        }

        auto myStart = aStart;
        auto myStop  = aStop;

        if (aStart > 0 and size_t(aStart) >= aContainer.size()) {
          return;
        } else if (aStart < 0) {
          // detail::safeAbs avoids the signed-negation UB when a
          // client passes INT64_MIN -- bare `-aStart` overflows.
          if (detail::safeAbs(aStart) > aContainer.size()) {
            myStart = 0;
          } else {
            myStart = aContainer.size() + aStart;
          }
        }

        if (aStop > 0 and size_t(aStop) >= aContainer.size()) {
          myStop = aContainer.size() - 1;
        } else if (aStop < 0) {
          // detail::safeAbs avoids INT64_MIN signed-negation UB.
          if (detail::safeAbs(aStop) > aContainer.size()) {
            return;
          } else {
            myStop = aContainer.size() + aStop;
          }
        }

        if (myStart > myStop) {
          return;
        }

        // Exact size known here — single allocation, no log N regrowth.
        myRet.reserve(static_cast<size_t>(myStop - myStart + 1));

        auto myItStart = aContainer.begin();
        std::advance(myItStart, myStart);
        auto myItStop = aContainer.begin();
        std::advance(myItStop, myStop + 1);

        while (myItStart != myItStop) {
          myRet.emplace_back(*myItStart);
          ++myItStart;
        }
      });

  return myRet;
}

template <class CONTAINER>
size_t SequenceContainer<CONTAINER>::remove(const std::string& aName,
                                            int64_t            aCount,
                                            const std::string& aValue) {
  size_t myRet = 0;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, aCount, &aValue](Container& aContainer) {
        if (aCount == 0) {
          size_t myRemoved = 0;
          for (auto myIt = aContainer.begin(); myIt != aContainer.end();) {
            if (*myIt == aValue) {
              myIt = aContainer.erase(myIt);
              ++myRemoved;
            } else {
              ++myIt;
            }
          }
          myRet = myRemoved;
        } else {

          uint64_t myToRemove = detail::safeAbs(aCount);
          const uint64_t myInitialToRemove = myToRemove;

          if (aCount > 0) {
            for (auto myIt = aContainer.begin();
                 myIt != aContainer.end() and myToRemove > 0;) {
              if (*myIt == aValue) {
                myIt = aContainer.erase(myIt);
                --myToRemove;
              } else {
                ++myIt;
              }
            }
          } else {
            for (auto myIt = aContainer.rbegin();
                 myIt != aContainer.rend() and myToRemove > 0;) {
              if (*myIt == aValue) {
                myIt = typename Container::reverse_iterator(
                    aContainer.erase(std::next(myIt).base()));
                --myToRemove;
              } else {
                ++myIt;
              }
            }
          }

          assert(myInitialToRemove >= myToRemove);
          myRet = static_cast<size_t>(myInitialToRemove - myToRemove);
        }
      });

  return myRet;
}

template <class CONTAINER>
typename SequenceContainer<CONTAINER>::Status SequenceContainer<CONTAINER>::set(
    const std::string& aName, int64_t aIndex, const std::string& aValue) {

  // Default to NOT_FOUND; the lambda below promotes to OK or
  // OUT_OF_RANGE only if the key actually exists. This is clearer than
  // the previous initial=OK + sentinel-bool pattern (where the only
  // signal that the lambda ran was a separate myFound flag).
  Status myRet = Status::NOT_FOUND;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, &aValue, aIndex](Container& aContainer) {
        if (aIndex >= 0) {
          if (size_t(aIndex) < aContainer.size()) {
            auto myIt = aContainer.begin();
            std::advance(myIt, aIndex);
            *myIt  = aValue;
            myRet  = Status::OK;
          } else {
            myRet = Status::OUT_OF_RANGE;
          }
        } else {
          // detail::safeAbs over int64 is defined for every input
          // including INT64_MIN; std::abs(INT64_MIN) is UB.
          auto myReverseIndex = static_cast<size_t>(detail::safeAbs(aIndex + 1));
          if (myReverseIndex < aContainer.size()) {
            auto myIt = aContainer.rbegin();
            std::advance(myIt, myReverseIndex);
            *myIt  = aValue;
            myRet  = Status::OK;
          } else {
            myRet = Status::OUT_OF_RANGE;
          }
        }
      });

  return myRet;
}

template <class CONTAINER>
void SequenceContainer<CONTAINER>::trim(const std::string& aName,
                                        int64_t            aStart,
                                        int64_t            aStop) {
  Base::theApplyer.performOnExisting(
      aName, [aStart, aStop](Container& aContainer) {
        if (aContainer.empty()) {
          return;
        }

        auto myStart = aStart;
        auto myStop  = aStop;

        if (aStart > 0 and size_t(aStart) >= aContainer.size()) {
          aContainer.clear();
          return;
        } else if (aStart < 0) {
          // detail::safeAbs avoids the signed-negation UB when a
          // client passes INT64_MIN -- bare `-aStart` overflows.
          if (detail::safeAbs(aStart) > aContainer.size()) {
            myStart = 0;
          } else {
            myStart = aContainer.size() + aStart;
          }
        }

        if (aStop > 0 and size_t(aStop) >= aContainer.size()) {
          myStop = aContainer.size() - 1;
        } else if (aStop < 0) {
          // detail::safeAbs avoids INT64_MIN signed-negation UB.
          if (detail::safeAbs(aStop) > aContainer.size()) {
            return;
          } else {
            myStop = aContainer.size() + aStop;
          }
        }

        if (myStart > myStop) {
          aContainer.clear();
          return;
        }

        if (myStart == 0 and size_t(myStop + 1) == aContainer.size()) {
          return;
        }

        auto myItStart = aContainer.begin();
        std::advance(myItStart, myStart);
        aContainer.erase(aContainer.begin(), myItStart);

        auto myItStop = aContainer.begin();
        std::advance(myItStop, myStop + 1 - myStart);
        aContainer.erase(myItStop, aContainer.end());
      });
}

template <class CONTAINER>
size_t SequenceContainer<CONTAINER>::pushBackExist(
    const std::string& aName, const std::vector<std::string_view>& aValues) {

  size_t myRet = 0;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, &aValues](Container& aContainer) {
        for (const auto& myString : aValues) {
          aContainer.emplace_back(myString);
        }
        myRet = aContainer.size();
      });

  return myRet;
}

template <class CONTAINER>
std::optional<std::string>
SequenceContainer<CONTAINER>::tryPopFrontOrWait(
    const std::string&     aName,
    BlockingWaiter::OnWake aOnWake,
    okts::stor::WaiterId*  aWaiterId) {
  okts::stor::BlockingWaiter myWaiter;
  myWaiter.onWake     = std::move(aOnWake);
  myWaiter.wantsFront = true;
  return Base::theApplyer.tryPopOrRegisterWaiter(
      aName,
      [](Container& aContainer) -> std::optional<std::string> {
        if (aContainer.empty()) return std::nullopt;
        std::string myValue = std::move(aContainer.front());
        aContainer.pop_front();
        return myValue;
      },
      std::move(myWaiter),
      aWaiterId);
}

template <class CONTAINER>
std::optional<std::string>
SequenceContainer<CONTAINER>::tryPopBackOrWait(
    const std::string&     aName,
    BlockingWaiter::OnWake aOnWake,
    okts::stor::WaiterId*  aWaiterId) {
  okts::stor::BlockingWaiter myWaiter;
  myWaiter.onWake     = std::move(aOnWake);
  myWaiter.wantsFront = false;
  return Base::theApplyer.tryPopOrRegisterWaiter(
      aName,
      [](Container& aContainer) -> std::optional<std::string> {
        if (aContainer.empty()) return std::nullopt;
        std::string myValue = std::move(aContainer.back());
        aContainer.pop_back();
        return myValue;
      },
      std::move(myWaiter),
      aWaiterId);
}

template <class CONTAINER>
std::vector<std::string> SequenceContainer<CONTAINER>::multiplePop(
    const std::vector<std::string>& aNames,
    Direction                       aDirection,
    uint64_t                        aCount) {
  std::vector<std::string> myRet;

  for (const auto& myName : aNames) {
    if (aDirection == Direction::LEFT) {
      myRet = popFront(myName, aCount);
    } else {
      myRet = popBack(myName, aCount);
    }

    if (!myRet.empty()) {
      break;
    }
  }

  return myRet;
}

} // namespace okts::stor
