#pragma once

#include "Support/noncopyable.h"

#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>

#include <absl/base/thread_annotations.h>

#include <functional>
#include <optional>
#include <unordered_map>

namespace okts {
namespace sup {

template <class CONTAINER>
class ContainerFunctorApplier
{
  using Container    = CONTAINER;
  using Functor      = std::function<void(Container& aList)>;
  using ConstFunctor = std::function<void(const Container& aList)>;

 public:
  DISABLE_EVIL_CONSTRUCTOR(ContainerFunctorApplier);

  ContainerFunctorApplier()
      : theMutex()
      , theStorage() {
  }

  size_t hostedKeys() const;

  // Apply the aFunctor on a "possibly" new container (if the container at the
  // given key aName doesn't exist it's then created). Functor should not
  // consume the container.
  void performOnNew(const std::string_view& aName, const Functor& aFunctor);

  // Apply the aFunction to an already existing container (as a pop),
  // functor can make the container empty, in that case the container is
  // removed.
  void performOnExisting(const std::string_view& aName,
                         const Functor&          aFunctor);

  // Apply the aFunction to an already existing container (as a
  // legth), functor takes the container as const hence the list can not be
  // modified.
  void performOnExisting(const std::string_view& aName,
                         const ConstFunctor&     aFunctor) const;

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

  // Maps a name to the actual container.
  using Storage = std::unordered_map<std::string,
                                     ProtectedContainer,
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
void ContainerFunctorApplier<CONTAINER>::performOnNew(
    const std::string_view& aName, const Functor& aFunctor) {

  ProtectedContainer* myContainer = nullptr;

  std::optional<boost::unique_lock<ContainerMutex>> mySecondLevelLock;

  while (true) {
    boost::lock_guard myLock(theMutex);

    myContainer = &theStorage[std::string(aName)];
    mySecondLevelLock.emplace(*myContainer->mutex, boost::try_to_lock_t());
    if (mySecondLevelLock->owns_lock()) {
      break;
    }
  }

  assert(mySecondLevelLock->owns_lock());
  aFunctor(myContainer->storage);
}

template <class CONTAINER>
void ContainerFunctorApplier<CONTAINER>::performOnExisting(
    const std::string_view& aName, const Functor& aFunctor) {

  // This looks a bit gymnic but we need to obey to the following:
  // after the operation if the list is empty has to be removed
  // from the storage.
  // The lock is done in multiple phases
  //  - External Lock
  //  - Try to acquire the internal Lock (each key has his own mutex), if the
  //  lock
  //    is not acquired it unlock the external lock and restart the acquisition,
  //    this will permit other threads to work on different keys
  //  - Unlock External Lock this gives the ability to another thread to operate
  //    on another key while current threads works on aName key
  //  - After the operation if the list has become empty has to be removed, but
  //    in order to do so the external lock has to be acquired, so release all
  //    the lock and start to:
  //      - Acquire external lock
  //      - Retrieve the key (if not exists we are done)
  //      - Lock the mutex (the mutex associated with the key)
  //      - If the key is still empty (in mean while while all locks were
  //      released some
  //        clients can have added a value) continue otherwise we are done
  //      - Move the mutex out on a local mutex (the key "carrying" the mutex is
  //      going
  //        to be destroyed hence we need to move it out)
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
      myContainer = &myIt->second;
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

    // The following mutex needs to be declared *before* the following lock,
    // indeed in case we do destroy a container we need to have the mutex
    // surviving his lock.
    std::unique_ptr<ContainerMutex>   myMutex;
    boost::lock_guard<ContainerMutex> mySecondLevelLock(*myIt->second.mutex);

    // If in the mean time the container has got new entries we can not destroy
    // it.
    if (not myIt->second.storage.empty()) {
      return;
    }

    // We are destroying the container and the associated mutex, move the mutex
    // and destroy the container.
    myMutex = std::move(myIt->second.mutex);
    theStorage.erase(myIt);
  }
}

template <class CONTAINER>
void ContainerFunctorApplier<CONTAINER>::performOnExisting(
    const std::string_view& aName, const ConstFunctor& aFunctor) const {

  const ProtectedContainer* myContainer = nullptr;

  std::optional<boost::unique_lock<ContainerMutex>> mySecondLevelLock;

  while (true) {
    boost::lock_guard myLock(theMutex);

    auto myIt = theStorage.find(aName);
    if (myIt == theStorage.end()) {
      return;
    }
    myContainer = &myIt->second;
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
