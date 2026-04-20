#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include <absl/container/flat_hash_map.h>

#include <glog/logging.h>

#include "Support/noncopyable.h"

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

 private:
  // Plain non-recursive mutex. The only command that used to re-enter
  // the per-key lock was LMOVE / RPOPLPUSH with source == destination,
  // and that case is now handled directly inside SequenceContainer::move
  // under a single lock acquisition.
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

  struct Shard {
    mutable std::mutex mutex;
    Storage            storage;
  };

  // Power of two so we can use bitmask routing.
  static constexpr size_t kShards = 32;
  static_assert((kShards & (kShards - 1)) == 0, "kShards must be a power of two");

  Shard&       shardFor(std::string_view aName);
  const Shard& shardFor(std::string_view aName) const;

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
    std::lock_guard<std::mutex> myLock(myShard.mutex);
    myCount += myShard.storage.size();
  }
  return myCount;
}

template <class CONTAINER>
void ContainerFunctorApplier<CONTAINER>::clear() {
  for (auto& myShard : theShards) {
    std::lock_guard<std::mutex> myLock(myShard.mutex);
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
    std::lock_guard<std::mutex> myLock(myShard.mutex);

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
          LOG(INFO) << "Inserted new container at key \"" << aName << "\"";
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
      std::lock_guard<std::mutex> myLock(myShard.mutex);

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
    std::lock_guard<std::mutex> myLock(myShard.mutex);

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

    myShard.storage.erase(myIt);
    LOG(INFO) << "Removed container at key \"" << aName << "\"";
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
    std::lock_guard<std::mutex> myLock(myShard.mutex);

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

} // namespace sup
} // namespace okts
