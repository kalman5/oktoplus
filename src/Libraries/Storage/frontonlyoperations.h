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
  DISABLE_EVIL_CONSTRUCTOR(FrontOnlyOperations);

  FrontOnlyOperations()
      : Base() {
  }

  size_t pushFront(const std::string&                   aName,
                   const std::vector<std::string_view>& aValues);

  std::optional<std::string> popFront(const std::string& aName);

  size_t pushFrontExist(const std::string&                   aName,
                        const std::vector<std::string_view>& aValues);

  std::optional<std::string>
  popBackPushFront(const std::string& aSourceName,
                   const std::string& aDestinationName);

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
std::optional<std::string> FrontOnlyOperations<CONTAINER>::popBackPushFront(
    const std::string& aSourceName, const std::string& aDestinationName) {

  std::optional<std::string> myRet;

  // clang-format off
  // The following mutex is required to avoid a dead lock in case two different
  // operations are running concurrently:
  //     popBackPushFront(L1 -> L2)
  //     popBackPushFront(L2 -> L1)
  // TODO: this has to be improved indeed it avoid the concurrency
  //       of every other popBackPushFront operations on different
  //       containers
  // clang-format on

  const boost::lock_guard myLock(thePopBackPushFrontMutex);

  Base::theApplyer.performOnExisting(
      aSourceName,
      [this, &aDestinationName, &myRet](Container& aSourceContainer) {
        if (aSourceContainer.empty()) {
          return;
        }
        std::string myValue = aSourceContainer.back();
        aSourceContainer.pop_back();

        Base::theApplyer.performOnNew(
            aDestinationName, [&myValue](Container& aDestinationContainer) {
              aDestinationContainer.push_front(myValue);
            });

        myRet = std::move(myValue);
      });

  return myRet;
}

} // namespace okts::stor
