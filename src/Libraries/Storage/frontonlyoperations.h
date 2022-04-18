#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include <boost/thread/mutex.hpp>

#include "Storage/genericcontainer.h"
#include "Support/noncopyable.h"

namespace okts::stor {

template <class CONTAINER>
class FrontOnlyOperations : virtual public GenericContainer<CONTAINER>
{
  using Container = CONTAINER;
  using Base      = GenericContainer<Container>;

 public:
  using Direction = typename Base::Direction;

  DISABLE_EVIL_CONSTRUCTOR(FrontOnlyOperations);

  FrontOnlyOperations()
      : Base() {
  }

  size_t pushFront(const std::string&                   aName,
                   const std::vector<std::string_view>& aValues);

  std::optional<std::string> popFront(const std::string& aName);

  size_t pushFrontExist(const std::string&                   aName,
                        const std::vector<std::string_view>& aValues);

  std::optional<std::string> move(const std::string& aSourceName,
                                  const std::string& aDestinationName,
                                  Direction          aSourceDirection,
                                  Direction          aDestinationDirection);

 private:
  using PopBackPushFrontMutex = boost::mutex;
  PopBackPushFrontMutex thePopBackPushFrontMutex;
};

//// INLINE DEFINITIONS

template <class CONTAINER>
size_t FrontOnlyOperations<CONTAINER>::pushFront(
    const std::string& aName, const std::vector<std::string_view>& aValues) {

  size_t myRet;

  Base::theApplyer.performOnNew(
      aName, [&myRet, &aValues](Container& aContainer) {
        for (const auto& myString : aValues) {
          aContainer.push_front(std::string(myString));
        }
        myRet = aContainer.size();
      });

  return myRet;
}

template <class CONTAINER>
std::optional<std::string>
FrontOnlyOperations<CONTAINER>::popFront(const std::string& aName) {

  std::optional<std::string> myRet;

  Base::theApplyer.performOnExisting(aName, [&myRet](Container& aContainer) {
    if (aContainer.empty()) {
      return;
    }
    myRet = aContainer.front();
    aContainer.pop_front();
  });

  return myRet;
}

template <class CONTAINER>
size_t FrontOnlyOperations<CONTAINER>::pushFrontExist(
    const std::string& aName, const std::vector<std::string_view>& aValues) {

  size_t myRet = 0;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, &aValues](Container& aContainer) {
        for (const auto& myString : aValues) {
          aContainer.push_front(std::string(myString));
        }
        myRet = aContainer.size();
      });

  return myRet;
}

template <class CONTAINER>
std::optional<std::string>
FrontOnlyOperations<CONTAINER>::move(const std::string& aSourceName,
                                     const std::string& aDestinationName,
                                     Direction          aSourceDirection,
                                     Direction          aDestinationDirection) {

  std::optional<std::string> myRet;

  // clang-format off
  // The following mutex is required to avoid a dead lock in case two different
  // operations are running concurrently:
  //     move(L1 -> L2)
  //     move(L2 -> L1)
  // TODO: this has to be improved indeed it avoid the concurrency
  //       of every other move operations on different
  //       containers
  // clang-format on

  const boost::lock_guard myLock(thePopBackPushFrontMutex);

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

} // namespace okts::stor
