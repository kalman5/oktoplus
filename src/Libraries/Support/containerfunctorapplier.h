#pragma once

#include <memory>
#include <optional>
#include <utility>

#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>

#include <absl/base/thread_annotations.h>
#include <absl/container/flat_hash_map.h>

#include <glog/logging.h>

#include "Support/noncopyable.h"

namespace okts {
namespace sup {

template <class CONTAINER>
class ContainerFunctorApplier
{
  using Container = CONTAINER;

 public:
  DISABLE_EVIL_CONSTRUCTOR(ContainerFunctorApplier);

  ContainerFunctorApplier()
      : theMutex()
      , theStorage() {
  }

  size_t hostedKeys() const;

  // Drop every container. Blocks until all per-key locks are observed
  // free; intended for FLUSHDB / FLUSHALL and test resets, not the hot
  // request path.
  void clear();

  // Apply aFunctor to a "possibly" new container (if the container at the
  // given key aName doesn't exist it's created). Functor must not consume
  // the container. Templated to avoid std::function heap-allocs on the
  // hot path; F is invocable as F(Container&).
  template <class F>
  void performOnNew(const std::string& aName, F&& aFunctor);

  // Apply aFunctor to an already existing container (e.g. pop). Functor
  // may leave the container empty, in which case it's removed.
  template <class F>
  void performOnExisting(const std::string& aName, F&& aFunctor);

  // Apply aFunctor to an already existing container, read-only. Functor
  // takes a const Container& so the container cannot be modified.
  template <class F>
  void performOnExisting(const std::string& aName, F&& aFunctor) const;

 private:
  // This needs to be recursive indeed some operations can work on multiple
  // containers at the same time (see operations pop back push front), the
  // two containers involved could be the same instance.
  using ContainerMutex = boost::recursive_mutex;

  struct ProtectedContainer {
    ProtectedContainer()
        : mutex(std::make_unique<ContainerMutex>())
        , storage() {
    }
    // At a certain point it needs to be moved out.
    std::unique_ptr<ContainerMutex> mutex;
    Container                       storage;
  };

  struct string_hash {
    using is_transparent = void; // Pred to use
    size_t operator()(std::string_view txt) const {
      return std::hash<std::string_view>{}(txt);
    }
    size_t operator()(const std::string& txt) const {
      return std::hash<std::string>{}(txt);
    }
  };

  // Maps a name to the actual container, value held in a unique_ptr so
  // raw pointers into the container survive rehash of the outer table
  // (flat_hash_map relocates entries on rehash; the unique_ptr indirection
  // keeps the ProtectedContainer at a stable heap address).
  using Storage = absl::flat_hash_map<std::string,
                                      std::unique_ptr<ProtectedContainer>,
                                      string_hash,
                                      std::equal_to<>>;

  // The following mutex protects the Storage container (each container
  // has his own ProtectedContainer).
  mutable boost::mutex theMutex;

  Storage theStorage ABSL_GUARDED_BY(theMutex);
};

template <class CONTAINER>
size_t ContainerFunctorApplier<CONTAINER>::hostedKeys() const {
  const boost::lock_guard myLock(theMutex);
  return theStorage.size();
}

template <class CONTAINER>
void ContainerFunctorApplier<CONTAINER>::clear() {
  const boost::lock_guard myLock(theMutex);
  // Each ProtectedContainer destructor releases its mutex when no one
  // holds it; if a per-key op is in flight we wait for the outer mutex
  // and then for the inner.
  for (auto& myEntry : theStorage) {
    boost::lock_guard<ContainerMutex> myInner(*myEntry.second->mutex);
    myEntry.second->storage.clear();
  }
  theStorage.clear();
}

template <class CONTAINER>
template <class F>
void ContainerFunctorApplier<CONTAINER>::performOnNew(const std::string& aName,
                                                      F&& aFunctor) {

  ProtectedContainer* myContainer = nullptr;

  std::optional<boost::unique_lock<ContainerMutex>> mySecondLevelLock;

  while (true) {
    boost::lock_guard myLock(theMutex);

    auto [myIterator, myInserted] = theStorage.try_emplace(aName);
    if (myInserted) {
      myIterator->second = std::make_unique<ProtectedContainer>();
      LOG(INFO) << "Inserted new container at key \"" << aName << "\"";
    }
    myContainer = myIterator->second.get();
    mySecondLevelLock.emplace(*myContainer->mutex, boost::try_to_lock_t());
    if (mySecondLevelLock->owns_lock()) {
      break;
    }
  }

  assert(mySecondLevelLock->owns_lock());
  aFunctor(myContainer->storage);
}

template <class CONTAINER>
template <class F>
void ContainerFunctorApplier<CONTAINER>::performOnExisting(
    const std::string& aName, F&& aFunctor) {

  // This looks a bit gymnic but we need to obey to the following:
  // after the operation if the list is empty has to be removed from storage.
  // The lock is done in multiple phases
  //  - External Lock.
  //  - Try to acquire the internal Lock (each key has his own mutex), if the
  //    lock is not acquired (another operation is being performed on same
  //    container) it unlock the external lock and restart the acquisition
  //    phase, this will permit other threads to work on different keys.
  //  - Unlock External Lock this gives the ability to another thread to operate
  //    on another key while current thread works on aName key.
  //  - After the operation if the list has become empty has to be removed, but
  //    in order to do so the external lock has to be acquired, so release all
  //    the lock and start to:
  //      - Acquire external lock
  //      - Retrieve the key (if not exists we are done)
  //      - Lock the mutex (the mutex associated with the key)
  //      - If the key is still empty (in mean while while all locks were
  //        released some clients can have added a value) continue otherwise we
  //        are done
  //      - Move the mutex out on a local mutex (the key "carrying" the mutex is
  //        going to be destroyed hence we need to move it out)
  //      - Remove the keys
  //      - Release all the locks

  bool myHasBecomeEmpty = false;

  {
    ProtectedContainer*                               myContainer = nullptr;
    std::optional<boost::unique_lock<ContainerMutex>> mySecondLevelLock;

    while (true) {
      boost::lock_guard myLock(theMutex);

      auto myIt = theStorage.find(aName);
      if (myIt == theStorage.end()) {
        return;
      }
      myContainer = myIt->second.get();
      mySecondLevelLock.emplace(*myContainer->mutex, boost::try_to_lock_t());
      if (mySecondLevelLock->owns_lock()) {
        break;
      }
    }

    assert(mySecondLevelLock->owns_lock());
    aFunctor(myContainer->storage);
    myHasBecomeEmpty = myContainer->storage.empty();
  }

  if (myHasBecomeEmpty) {
    boost::lock_guard myLock(theMutex);

    // check if the container has not been already removed.
    auto myIt = theStorage.find(aName);
    if (myIt == theStorage.end()) {
      return;
    }

    // Move the unique_ptr<ProtectedContainer> out of the map so the
    // ProtectedContainer (and its embedded mutex) outlives the
    // lock_guard that's about to lock it. Destruction of the
    // unique_ptr happens at function scope exit, after the lock_guard
    // destructor releases the inner mutex.
    std::unique_ptr<ProtectedContainer> myEvicted = std::move(myIt->second);
    boost::lock_guard<ContainerMutex>   mySecondLevelLock(*myEvicted->mutex);

    // If in the mean time the container has got new entries we can not destroy
    // it.
    if (not myEvicted->storage.empty()) {
      // Put it back. (Rare; race window is tiny.)
      myIt->second = std::move(myEvicted);
      return;
    }

    theStorage.erase(myIt);
    LOG(INFO) << "Removed container at key \"" << aName << "\"";
  }
}

template <class CONTAINER>
template <class F>
void ContainerFunctorApplier<CONTAINER>::performOnExisting(
    const std::string& aName, F&& aFunctor) const {

  const ProtectedContainer* myContainer = nullptr;

  std::optional<boost::unique_lock<ContainerMutex>> mySecondLevelLock;

  while (true) {
    boost::lock_guard myLock(theMutex);

    auto myIt = theStorage.find(aName);
    if (myIt == theStorage.end()) {
      return;
    }
    myContainer = myIt->second.get();
    mySecondLevelLock.emplace(*myContainer->mutex, boost::try_to_lock_t());
    if (mySecondLevelLock->owns_lock()) {
      break;
    }
  }

  assert(mySecondLevelLock->owns_lock());
  aFunctor(myContainer->storage);
}

} // namespace sup
} // namespace okts
