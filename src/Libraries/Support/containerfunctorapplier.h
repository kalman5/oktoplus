#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include <glog/logging.h>

#include "Storage/blocking_waiter.h"
#include "Support/noncopyable.h"
#include "Support/spinlock.h"

namespace okts {
namespace sup {

// Holds a map of key -> Container with per-key mutual exclusion.
//
// The keyspace is sharded across kShards independent (mutex + map)
// pairs. Each command picks its shard by hashing the key, so commands
// on different keys mostly hit different shards and stop serialising
// on a single global outer mutex. Inside a shard, the existing
// outer-then-inner try-lock-retry pattern is preserved.
//
// The value type is unique_ptr<ProtectedContainer>: flat_hash_map
// relocates entries on rehash, but the heap-allocated
// ProtectedContainer keeps its address stable, so raw pointers (used
// to drop the outer lock before running the functor) stay valid.
template <class CONTAINER>
class ContainerFunctorApplier
{
  using Container = CONTAINER;

 public:
  DISABLE_EVIL_CONSTRUCTOR(ContainerFunctorApplier);

  ContainerFunctorApplier() = default;

  size_t hostedKeys() const;

  // Drop every container. Intended for FLUSHDB / FLUSHALL and test
  // resets, not the hot request path.
  //
  // WARNING: takes the outer mutex of every shard sequentially, and
  // for each shard takes the inner per-key mutex of every container
  // before clearing. While running, no other operation on any key
  // can make progress (every shard outer is locked, then each inner
  // is taken). RESP exposes this via FLUSHDB / FLUSHALL with no rate
  // limit — a malicious client can wedge all worker threads. Acceptable
  // today because FLUSHDB is intentionally heavy (it is in Redis too),
  // but worth knowing if exposing to untrusted networks.
  void clear();

  // Apply aFunctor to a "possibly" new container (created on demand).
  // Functor must not consume the container. Templated to avoid
  // std::function heap-allocs on the hot path; F is invocable as
  // F(Container&).
  template <class F>
  void performOnNew(const std::string& aName, F&& aFunctor);

  // Apply aFunctor to an existing container (e.g. pop). If the functor
  // leaves the container empty, the entry is removed from the map.
  template <class F>
  void performOnExisting(const std::string& aName, F&& aFunctor);

  // Apply aFunctor to an existing container, read-only.
  template <class F>
  void performOnExisting(const std::string& aName, F&& aFunctor) const;

  // ---- Blocking-waiter API (BLPOP / BRPOP / BLMPOP / BLMOVE / BRPOPLPUSH) -

  // Register a waiter on the given key, creating an empty entry if
  // the key doesn't exist. Returns the new waiter's id; pass it to
  // cancelWaiter() if a timeout fires or the client disconnects
  // before the waiter is woken naturally.
  //
  // The caller must already have *failed* a non-blocking pop attempt
  // before registering — this method does not retry to drain
  // anything; it just enqueues the waiter.
  okts::stor::WaiterId registerWaiter(const std::string&         aName,
                                      okts::stor::BlockingWaiter aWaiter);

  // Atomic "try non-blocking pop, otherwise register a waiter". The
  // single lock acquisition closes the race where a producer pushes
  // between an unlocked pop attempt and a follow-up registerWaiter.
  //
  //   aTryPop(storage) returns std::optional<std::string> -- the
  //                    popped value if storage was non-empty,
  //                    std::nullopt otherwise. The caller chooses
  //                    front-vs-back via this lambda body.
  //
  // On success (pop): returns the value, *aWaiterId is set to 0
  // (nothing to cancel).
  // On registration: returns std::nullopt, *aWaiterId is set to the
  // new waiter's id.
  template <class TryPop>
  std::optional<std::string>
  tryPopOrRegisterWaiter(const std::string&         aName,
                         TryPop&&                   aTryPop,
                         okts::stor::BlockingWaiter aWaiter,
                         okts::stor::WaiterId*      aWaiterId);

  // Remove a registered waiter by id. Returns true if the waiter was
  // found and erased (i.e. the caller still owns the wake side and
  // must invoke its onWake itself with std::nullopt). Returns false
  // if the waiter was already woken / removed (the producer side
  // got there first; nothing to cancel).
  bool cancelWaiter(const std::string& aName, okts::stor::WaiterId aId);

  // Run aFunctor under the per-key inner lock, then drain any waiters
  // parked on this key. For each waiter (FIFO), pop from its
  // preferred end of the storage and hand the value to onWake; stop
  // when either the waiters list or the storage is empty. Finally,
  // run aFinalize(container) under the same lock so the caller can
  // observe the storage state *after* both push and drain (used by
  // LPUSH/RPUSH to capture the post-wake length for the reply).
  //
  // aPopFront and aPopBack must be invocable as
  // `std::optional<std::string> (Container&)` -- pop and return the
  // popped value, or nullopt if the storage is empty. They're passed
  // in (rather than calling storage.front()/pop_front() directly) so
  // this method stays generic across containers that store types
  // other than std::string.
  template <class F, class PopFront, class PopBack, class Finalize>
  void performAndDrainWaiters(const std::string& aName,
                              F&&                aFunctor,
                              PopFront&&         aPopFront,
                              PopBack&&          aPopBack,
                              Finalize&&         aFinalize);

 private:
  // Plain non-recursive mutex. The only command that used to re-enter
  // the SAME per-key lock was LMOVE / RPOPLPUSH with source ==
  // destination, and that case is now handled directly inside
  // SequenceContainer::move under a single lock acquisition.
  //
  // Note: cross-key LMOVE still acquires *two* per-key inner mutexes
  // (one per key) inside its outer functor, but those are different
  // ProtectedContainer instances, so they don't re-enter the same
  // mutex — non-recursive is fine. theMoveMutex serializes such moves
  // to prevent the L1<->L2 deadlock; see SequenceContainer::move.
  using ContainerMutex = std::mutex;

  struct ProtectedContainer {
    // Inline mutex — saves one heap allocation per new key on the
    // hot insertion path. Safe even though we erase the map entry
    // while holding the lock: the eviction code moves the entire
    // unique_ptr<ProtectedContainer> out of the map slot first, so
    // the ProtectedContainer (and its embedded mutex) outlives the
    // lock_guard that referenced it.
    //
    // mutable so the const-overload of performOnExisting can lock
    // the mutex through a `const ProtectedContainer*` view.
    mutable ContainerMutex mutex;
    Container              storage;
    // Clients suspended on BLPOP / BRPOP / etc. against this key.
    // Lazy-allocated because almost every key has zero waiters; the
    // 8-byte unique_ptr keeps per-key overhead low for non-blocking
    // workloads. Manipulated only under `mutex`. std::list is used
    // for stable iterators so a Waiter can be removed by handle on
    // timeout or disconnect.
    std::unique_ptr<std::list<okts::stor::BlockingWaiter>> waiters;
  };

  struct string_hash {
    using is_transparent = void;
    size_t operator()(std::string_view txt) const {
      return std::hash<std::string_view>{}(txt);
    }
    size_t operator()(const std::string& txt) const {
      return std::hash<std::string>{}(txt);
    }
  };

  using Storage = absl::flat_hash_map<std::string,
                                      std::unique_ptr<ProtectedContainer>,
                                      string_hash,
                                      std::equal_to<>>;

  // Outer shard mutex is a user-space TTAS spinlock with no
  // scheduler yield (BareSpinlock; see Support/spinlock.h).
  // Profiling under a hot-key workload (50 clients hammering one key,
  // -P 16) showed ~70% of CPU spent in the kernel's
  // queued_spin_lock_slowpath via futex_wake on every std::mutex
  // unlock of this shard mutex. The critical section is just one
  // absl::flat_hash_map lookup -- microseconds at worst -- so
  // spinning is cheaper than the kernel round trip a futex would
  // cost. Switching this took hot-key LPUSH at -c 50 -P 16 from
  // ~22% of Redis to ~100%. The inner per-key mutex stays std::mutex
  // so long-running ops (LRANGE on huge lists, LPOS scans) block
  // waiters in the kernel rather than burning CPU spinning.
  using ShardMutex = BareSpinlock;
  struct Shard {
    mutable ShardMutex mutex;
    Storage            storage;
  };

  // Power of two so we can use bitmask routing.
  static constexpr size_t kShards = 32;
  static_assert((kShards & (kShards - 1)) == 0, "kShards must be a power of two");

  Shard&       shardFor(std::string_view aName);
  const Shard& shardFor(std::string_view aName) const;

  // Monotonically increasing waiter id source (for cancel-by-id
  // semantics). Bumps once per registerWaiter() call.
  std::atomic<okts::stor::WaiterId> theNextWaiterId{1};

  mutable std::array<Shard, kShards> theShards;
};

template <class CONTAINER>
typename ContainerFunctorApplier<CONTAINER>::Shard&
ContainerFunctorApplier<CONTAINER>::shardFor(std::string_view aName) {
  return theShards[std::hash<std::string_view>{}(aName) & (kShards - 1)];
}

template <class CONTAINER>
const typename ContainerFunctorApplier<CONTAINER>::Shard&
ContainerFunctorApplier<CONTAINER>::shardFor(std::string_view aName) const {
  return theShards[std::hash<std::string_view>{}(aName) & (kShards - 1)];
}

template <class CONTAINER>
size_t ContainerFunctorApplier<CONTAINER>::hostedKeys() const {
  size_t myCount = 0;
  for (const auto& myShard : theShards) {
    std::lock_guard<ShardMutex> myLock(myShard.mutex);
    myCount += myShard.storage.size();
  }
  return myCount;
}

template <class CONTAINER>
void ContainerFunctorApplier<CONTAINER>::clear() {
  for (auto& myShard : theShards) {
    std::lock_guard<ShardMutex> myLock(myShard.mutex);
    for (auto& myEntry : myShard.storage) {
      std::lock_guard<ContainerMutex> myInner(myEntry.second->mutex);
      myEntry.second->storage.clear();
    }
    myShard.storage.clear();
  }
}

template <class CONTAINER>
template <class F>
void ContainerFunctorApplier<CONTAINER>::performOnNew(const std::string& aName,
                                                      F&& aFunctor) {
  auto& myShard = shardFor(aName);

  ProtectedContainer*                myContainer = nullptr;
  std::unique_lock<ContainerMutex> mySecondLevelLock;

  while (true) {
    std::lock_guard<ShardMutex> myLock(myShard.mutex);

    // Single-hash find-or-insert via absl's lazy_emplace: the
    // transparent hasher hashes the string_view once; the lambda is
    // invoked only on miss to actually construct the slot. The hit
    // path costs one hash + one slot probe; the miss path adds one
    // slot construction. No temporary std::string is built either way.
    //
    // Compare with the previous shape (find then try_emplace on
    // miss) which hashed twice on the miss path because the
    // standard try_emplace doesn't expose the find result.
    auto myIterator = myShard.storage.lazy_emplace(
        std::string_view(aName),
        [&aName](const auto& aCtor) {
          aCtor(aName, std::make_unique<ProtectedContainer>());
          VLOG(2) << "Inserted new container at key \"" << aName << "\"";
        });
    myContainer       = myIterator->second.get();
    mySecondLevelLock = std::unique_lock<ContainerMutex>(myContainer->mutex,
                                                          std::try_to_lock);
    if (mySecondLevelLock.owns_lock()) {
      break;
    }
  }

  assert(mySecondLevelLock.owns_lock());
  aFunctor(myContainer->storage);
}

// This looks a bit gymnic but we need to obey to the following:
// after the operation if the list is empty has to be removed from
// storage. The lock is done in multiple phases:
//  - Pick the shard for aName (hash-routed, no contention with other
//    shards).
//  - Acquire the shard's outer mutex.
//  - Try to acquire the internal lock (each key has its own mutex),
//    if the lock is not acquired (another operation is being performed
//    on the same container) it unlocks the outer mutex and restarts
//    the acquisition phase, this will permit other threads to work on
//    different keys within the same shard.
//  - Release the outer mutex; this gives the ability to another thread
//    to operate on another key in this shard while the current thread
//    works on aName.
//  - After the operation, if the container has become empty it has to
//    be removed, but in order to do so the outer mutex has to be
//    re-acquired, so release all locks and start to:
//      - Acquire the outer mutex.
//      - Retrieve the key (if it no longer exists we are done).
//      - Lock the inner mutex (the mutex associated with the key).
//      - If the container is still empty (in the meanwhile, while all
//        locks were released, some client may have added a value)
//        continue, otherwise we are done.
//      - Move the unique_ptr<ProtectedContainer> out of the map slot
//        so that the embedded inner mutex outlives the lock_guard
//        about to lock it (the key carrying the mutex is going to be
//        destroyed hence we need to move it out).
//      - Erase the map entry.
//      - Release all the locks (the moved-out unique_ptr is destroyed
//        at function scope, after the inner lock_guard).
template <class CONTAINER>
template <class F>
void ContainerFunctorApplier<CONTAINER>::performOnExisting(
    const std::string& aName, F&& aFunctor) {
  auto& myShard = shardFor(aName);

  bool myHasBecomeEmpty = false;

  {
    ProtectedContainer*                myContainer = nullptr;
    std::unique_lock<ContainerMutex> mySecondLevelLock;

    while (true) {
      std::lock_guard<ShardMutex> myLock(myShard.mutex);

      auto myIt = myShard.storage.find(aName);
      if (myIt == myShard.storage.end()) {
        return;
      }
      myContainer       = myIt->second.get();
      mySecondLevelLock = std::unique_lock<ContainerMutex>(myContainer->mutex,
                                                              std::try_to_lock);
      if (mySecondLevelLock.owns_lock()) {
        break;
      }
    }

    assert(mySecondLevelLock.owns_lock());
    aFunctor(myContainer->storage);
    myHasBecomeEmpty = myContainer->storage.empty();
  }

  if (myHasBecomeEmpty) {
    std::lock_guard<ShardMutex> myLock(myShard.mutex);

    auto myIt = myShard.storage.find(aName);
    if (myIt == myShard.storage.end()) {
      return;
    }

    // Move the unique_ptr<ProtectedContainer> out of the map slot so
    // its embedded mutex outlives the lock_guard that's about to lock
    // it. Destruction of the moved-out unique_ptr happens at function
    // scope exit, after the lock_guard destructor releases the inner
    // mutex.
    std::unique_ptr<ProtectedContainer> myEvicted = std::move(myIt->second);
    std::lock_guard<ContainerMutex>   mySecondLevelLock(myEvicted->mutex);

    if (not myEvicted->storage.empty()) {
      myIt->second = std::move(myEvicted);
      return;
    }

    // Don't evict if BLPOP/BRPOP-style waiters are still parked on
    // this key -- they're waiting for the next push, and the entry
    // has to outlive them so the producer can find them.
    if (myEvicted->waiters && !myEvicted->waiters->empty()) {
      myIt->second = std::move(myEvicted);
      return;
    }

    myShard.storage.erase(myIt);
    VLOG(2) << "Removed container at key \"" << aName << "\"";
  }
}

template <class CONTAINER>
template <class F>
void ContainerFunctorApplier<CONTAINER>::performOnExisting(
    const std::string& aName, F&& aFunctor) const {
  const auto& myShard = shardFor(aName);

  const ProtectedContainer*          myContainer = nullptr;
  std::unique_lock<ContainerMutex> mySecondLevelLock;

  while (true) {
    std::lock_guard<ShardMutex> myLock(myShard.mutex);

    auto myIt = myShard.storage.find(aName);
    if (myIt == myShard.storage.end()) {
      return;
    }
    myContainer       = myIt->second.get();
    mySecondLevelLock = std::unique_lock<ContainerMutex>(myContainer->mutex,
                                                            std::try_to_lock);
    if (mySecondLevelLock.owns_lock()) {
      break;
    }
  }

  assert(mySecondLevelLock.owns_lock());
  aFunctor(myContainer->storage);
}

template <class CONTAINER>
okts::stor::WaiterId
ContainerFunctorApplier<CONTAINER>::registerWaiter(
    const std::string& aName, okts::stor::BlockingWaiter aWaiter) {
  auto& myShard = shardFor(aName);

  ProtectedContainer*              myContainer = nullptr;
  std::unique_lock<ContainerMutex> mySecondLevelLock;

  while (true) {
    std::lock_guard<ShardMutex> myLock(myShard.mutex);

    // lazy_emplace creates an empty entry if the key doesn't exist
    // yet — the waiter has to live somewhere even when no list does.
    auto myIterator = myShard.storage.lazy_emplace(
        std::string_view(aName),
        [&aName](const auto& aCtor) {
          aCtor(aName, std::make_unique<ProtectedContainer>());
        });
    myContainer       = myIterator->second.get();
    mySecondLevelLock = std::unique_lock<ContainerMutex>(myContainer->mutex,
                                                          std::try_to_lock);
    if (mySecondLevelLock.owns_lock()) {
      break;
    }
  }

  assert(mySecondLevelLock.owns_lock());
  if (!myContainer->waiters) {
    myContainer->waiters =
        std::make_unique<std::list<okts::stor::BlockingWaiter>>();
  }
  const auto myId =
      theNextWaiterId.fetch_add(1, std::memory_order_relaxed);
  aWaiter.id = myId;
  myContainer->waiters->push_back(std::move(aWaiter));
  return myId;
}

template <class CONTAINER>
bool ContainerFunctorApplier<CONTAINER>::cancelWaiter(
    const std::string& aName, okts::stor::WaiterId aId) {
  auto& myShard = shardFor(aName);

  ProtectedContainer*              myContainer = nullptr;
  std::unique_lock<ContainerMutex> mySecondLevelLock;

  while (true) {
    std::lock_guard<ShardMutex> myLock(myShard.mutex);

    auto myIt = myShard.storage.find(aName);
    if (myIt == myShard.storage.end()) {
      return false;
    }
    myContainer       = myIt->second.get();
    mySecondLevelLock = std::unique_lock<ContainerMutex>(myContainer->mutex,
                                                          std::try_to_lock);
    if (mySecondLevelLock.owns_lock()) {
      break;
    }
  }

  if (!myContainer->waiters) {
    return false;
  }
  for (auto myWIt = myContainer->waiters->begin();
       myWIt != myContainer->waiters->end(); ++myWIt) {
    if (myWIt->id == aId) {
      myContainer->waiters->erase(myWIt);
      return true;
    }
  }
  return false;
}

template <class CONTAINER>
template <class TryPop>
std::optional<std::string>
ContainerFunctorApplier<CONTAINER>::tryPopOrRegisterWaiter(
    const std::string&         aName,
    TryPop&&                   aTryPop,
    okts::stor::BlockingWaiter aWaiter,
    okts::stor::WaiterId*      aWaiterId) {
  auto& myShard = shardFor(aName);

  ProtectedContainer*              myContainer = nullptr;
  std::unique_lock<ContainerMutex> mySecondLevelLock;

  while (true) {
    std::lock_guard<ShardMutex> myLock(myShard.mutex);

    auto myIterator = myShard.storage.lazy_emplace(
        std::string_view(aName),
        [&aName](const auto& aCtor) {
          aCtor(aName, std::make_unique<ProtectedContainer>());
        });
    myContainer       = myIterator->second.get();
    mySecondLevelLock = std::unique_lock<ContainerMutex>(myContainer->mutex,
                                                          std::try_to_lock);
    if (mySecondLevelLock.owns_lock()) {
      break;
    }
  }

  assert(mySecondLevelLock.owns_lock());

  // Try non-blocking pop first.
  auto myPopped = aTryPop(myContainer->storage);
  if (myPopped) {
    if (aWaiterId) {
      *aWaiterId = 0;
    }
    return myPopped;
  }

  // Empty: register as waiter.
  if (!myContainer->waiters) {
    myContainer->waiters =
        std::make_unique<std::list<okts::stor::BlockingWaiter>>();
  }
  const auto myId =
      theNextWaiterId.fetch_add(1, std::memory_order_relaxed);
  aWaiter.id = myId;
  myContainer->waiters->push_back(std::move(aWaiter));
  if (aWaiterId) {
    *aWaiterId = myId;
  }
  return std::nullopt;
}

template <class CONTAINER>
template <class F, class PopFront, class PopBack, class Finalize>
void ContainerFunctorApplier<CONTAINER>::performAndDrainWaiters(
    const std::string& aName,
    F&&                aFunctor,
    PopFront&&         aPopFront,
    PopBack&&          aPopBack,
    Finalize&&         aFinalize) {
  auto& myShard = shardFor(aName);

  // Collect waiters whose onWake we need to invoke after dropping the
  // lock. Holding the inner mutex while running an arbitrary callback
  // risks both deadlock (the callback might recursively touch storage)
  // and convoying (callbacks might stall behind unrelated I/O).
  std::vector<std::pair<okts::stor::BlockingWaiter,
                        std::optional<std::string>>> myFiring;

  {
    ProtectedContainer*              myContainer = nullptr;
    std::unique_lock<ContainerMutex> mySecondLevelLock;

    while (true) {
      std::lock_guard<ShardMutex> myLock(myShard.mutex);

      auto myIterator = myShard.storage.lazy_emplace(
          std::string_view(aName),
          [&aName](const auto& aCtor) {
            aCtor(aName, std::make_unique<ProtectedContainer>());
          });
      myContainer       = myIterator->second.get();
      mySecondLevelLock = std::unique_lock<ContainerMutex>(myContainer->mutex,
                                                            std::try_to_lock);
      if (mySecondLevelLock.owns_lock()) {
        break;
      }
    }

    assert(mySecondLevelLock.owns_lock());
    aFunctor(myContainer->storage);

    if (myContainer->waiters && !myContainer->waiters->empty()) {
      auto& myWList = *myContainer->waiters;
      while (!myWList.empty()) {
        auto& myWaiter = myWList.front();
        std::optional<std::string> myValue;
        if (myWaiter.wantsFront) {
          myValue = aPopFront(myContainer->storage);
        } else {
          myValue = aPopBack(myContainer->storage);
        }
        if (!myValue) {
          break; // storage drained
        }
        myFiring.emplace_back(std::move(myWaiter), std::move(myValue));
        myWList.pop_front();
      }
    }

    // Final hook so the caller can capture post-drain storage state
    // (e.g. LPUSH's return value) under the same lock.
    aFinalize(myContainer->storage);
  }

  // Lock released. Fire wake callbacks. Each onWake is expected to
  // post onto the waiter's owning io_context and return quickly.
  for (auto& myEntry : myFiring) {
    if (myEntry.first.onWake) {
      myEntry.first.onWake(std::move(myEntry.second));
    }
  }
}

} // namespace sup
} // namespace okts
