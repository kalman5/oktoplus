#pragma once

#include "Support/noncopyable.h"

#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>

#include <absl/base/thread_annotations.h>

#include <functional>
#include <unordered_map>

namespace okts {
namespace sup {

template <class CONTAINER>
class ContainerFunctorApplier
{
 public:
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

  void performOnNew(const std::string& aName, const Functor& aFunctor);
  void performOnExisting(const std::string& aName, const Functor& aFunctor);
  void performOnExisting(const std::string&  aName,
                         const ConstFunctor& aFunctor) const;

 private:
  using ContainerMutex = boost::recursive_mutex;

  struct ProtectedContainer {
    ProtectedContainer()
        : mutex(new ContainerMutex())
        , storage() {
    }
    std::unique_ptr<ContainerMutex> mutex;
    Container                       storage;
  };

  using Storage = std::unordered_map<std::string, ProtectedContainer>;

  mutable boost::mutex theMutex;

  Storage theStorage ABSL_GUARDED_BY(theMutex);
};

template <class CONTAINER>
size_t ContainerFunctorApplier<CONTAINER>::hostedKeys() const {

  const boost::lock_guard myLock(theMutex);
  return theStorage.size();
}

template <class CONTAINER>
void ContainerFunctorApplier<CONTAINER>::performOnNew(const std::string& aName,
                                                      const Functor& aFunctor) {

  ProtectedContainer*                                 myContainer = nullptr;
  std::unique_ptr<boost::unique_lock<ContainerMutex>> mySecondLevelLock;

  while (true) {
    boost::lock_guard myLock(theMutex);
    myContainer = &theStorage[aName];
    mySecondLevelLock.reset(new boost::unique_lock<ContainerMutex>(
        *myContainer->mutex, boost::try_to_lock_t()));
    if (mySecondLevelLock->owns_lock()) {
      break;
    }
  }

  assert(mySecondLevelLock->owns_lock());
  aFunctor(myContainer->storage);
}

template <class CONTAINER>
void ContainerFunctorApplier<CONTAINER>::performOnExisting(
    const std::string& aName, const Functor& aFunctor) {

  // This looks a bit gymnic but this is what needs to do
  // After the operation if the list is empty has to be removed
  // from the storage
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
    ProtectedContainer*                                 myContainer = nullptr;
    std::unique_ptr<boost::unique_lock<ContainerMutex>> mySecondLevelLock;

    while (true) {
      boost::lock_guard myLock(theMutex);

      auto myIt = theStorage.find(aName);
      if (myIt == theStorage.end()) {
        return;
      }
      myContainer = &myIt->second;
      mySecondLevelLock.reset(new boost::unique_lock<ContainerMutex>(
          *myContainer->mutex, boost::try_to_lock_t()));
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
    auto              myIt = theStorage.find(aName);
    if (myIt == theStorage.end()) {
      return;
    }
    std::unique_ptr<ContainerMutex>   myMutex;
    boost::lock_guard<ContainerMutex> mySecondLevelLock(*myIt->second.mutex);

    if (not myIt->second.storage.empty()) {
      return;
    }

    myMutex = std::move(myIt->second.mutex);
    theStorage.erase(myIt);
  }
}

template <class CONTAINER>
void ContainerFunctorApplier<CONTAINER>::performOnExisting(
    const std::string& aName, const ConstFunctor& aFunctor) const {

  const ProtectedContainer* myList = nullptr;

  std::unique_ptr<boost::unique_lock<ContainerMutex>> mySecondLevelLock;

  while (true) {
    boost::lock_guard myLock(theMutex);
    auto              myIt = theStorage.find(aName);
    if (myIt == theStorage.end()) {
      return;
    }
    myList = &myIt->second;
    mySecondLevelLock.reset(new boost::unique_lock<ContainerMutex>(
        *myList->mutex, boost::try_to_lock_t()));
    if (mySecondLevelLock->owns_lock()) {
      break;
    }
  }

  assert(mySecondLevelLock->owns_lock());
  aFunctor(myList->storage);
}

} // namespace sup
} // namespace okts
