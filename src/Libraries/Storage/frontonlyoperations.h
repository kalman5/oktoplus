#pragma once

#include <cstdint>
#include <list>
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

  std::list<std::string> popFront(const std::string& aName, uint64_t aCount);

  size_t pushFrontExist(const std::string&                   aName,
                        const std::vector<std::string_view>& aValues);
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
std::list<std::string>
FrontOnlyOperations<CONTAINER>::popFront(const std::string& aName,
                                         const uint64_t     aCount) {

  std::list<std::string> myRet;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, &aCount](Container& aContainer) {
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

} // namespace okts::stor
