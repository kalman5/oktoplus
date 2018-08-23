#include "Storage/lists.h"

#include <algorithm>
#include <iterator>
#include <string>

#include <boost/thread/lock_guard.hpp>

namespace oktoplus {
namespace storage {

Lists::Lists()
    : theApplyer()
    , thePopBackPushFrontMutex() {
}

size_t Lists::hostedKeys() const {
  return theApplyer.hostedKeys();
}

size_t Lists::pushFront(const std::string&                   aName,
                        const std::vector<std::string_view>& aValues) {

  size_t myRet;

  theApplyer.performOnNew(aName, [&myRet, &aValues](List& aList) {
    for (const auto& myString : aValues) {
      aList.push_front(std::string(myString));
    }
    myRet = aList.size();
  });

  return myRet;
}

size_t Lists::pushBack(const std::string&                   aName,
                       const std::vector<std::string_view>& aValues) {

  size_t myRet;

  theApplyer.performOnNew(aName, [&myRet, &aValues](List& aList) {
    for (const auto& myString : aValues) {
      aList.push_back(std::string(myString));
    }
    myRet = aList.size();
  });

  return myRet;
}

boost::optional<std::string> Lists::popBack(const std::string& aName) {

  boost::optional<std::string> myRet;

  theApplyer.performOnExisting(aName, [&myRet](List& aList) {
    if (aList.empty()) {
      return;
    }
    myRet = aList.back();
    aList.pop_back();
  });

  return myRet;
}

boost::optional<std::string> Lists::popFront(const std::string& aName) {

  boost::optional<std::string> myRet;

  theApplyer.performOnExisting(aName, [&myRet](List& aList) {
    if (aList.empty()) {
      return;
    }
    myRet = aList.front();
    aList.pop_front();
  });

  return myRet;
}

size_t Lists::size(const std::string& aName) const {

  size_t myRet = 0;

  theApplyer.performOnExisting(aName,
                    [&myRet](const List& aList) { myRet = aList.size(); });

  return myRet;
}

boost::optional<std::string> Lists::index(const std::string& aName,
                                          int64_t            aIndex) const {

  boost::optional<std::string> myRet;

  theApplyer.performOnExisting(aName, [&myRet, aIndex](const List& aList) {
    if (aIndex >= 0) {
      if (size_t(aIndex) < aList.size()) {
        auto myIt = aList.begin();
        std::advance(myIt, aIndex);
        myRet = *myIt;
      }
    } else {
      auto myReverseIndex = size_t(std::abs(aIndex + 1));
      if (myReverseIndex < aList.size()) {
        auto myIt = aList.rbegin();
        std::advance(myIt, myReverseIndex);
        myRet = *myIt;
      }
    }
  });

  return myRet;
}

boost::optional<int64_t> Lists::insert(const std::string& aName,
                                       Position           aPosition,
                                       const std::string& aPivot,
                                       const std::string& aValue) {
  boost::optional<int64_t> myRet;

  theApplyer.performOnExisting(aName, [&myRet, aPosition, &aPivot, &aValue](List& aList) {
    auto myIt = std::find(aList.begin(), aList.end(), aPivot);
    if (myIt == aList.end()) {
      myRet = -1;
      return;
    }

    if (aPosition == Position::AFTER) {
      ++myIt;
    }

    aList.insert(myIt, aValue);
    myRet = aList.size();
  });

  return myRet;
}

size_t Lists::pushFrontExist(const std::string&                   aName,
                             const std::vector<std::string_view>& aValues) {

  size_t myRet = 0;

  theApplyer.performOnExisting(aName, [&myRet, &aValues](List& aList) {
    for (const auto& myString : aValues) {
      aList.push_front(std::string(myString));
    }
    myRet = aList.size();
  });

  return myRet;
}

std::vector<std::string>
Lists::range(const std::string& aName, int64_t aStart, int64_t aStop) const {

  std::vector<std::string> myRet;

  theApplyer.performOnExisting(aName, [&myRet, aStart, aStop](const List& aList) {
    if (aList.empty()) {
      return;
    }

    auto myStart = aStart;
    auto myStop  = aStop;

    if (aStart > 0 and size_t(aStart) >= aList.size()) {
      return;
    } else if (aStart < 0) {
      if (size_t(-aStart) > aList.size()) {
        myStart = 0;
      } else {
        myStart = aList.size() + aStart;
      }
    }

    if (aStop > 0 and size_t(aStop) >= aList.size()) {
      myStop = aList.size() - 1;
    } else if (aStop < 0) {
      if (size_t(-aStop) > aList.size()) {
        return;
      } else {
        myStop = aList.size() + aStop;
      }
    }

    if (myStart > myStop) {
      return;
    }

    auto myItStart = aList.begin();
    std::advance(myItStart, myStart);
    auto myItStop = aList.begin();
    std::advance(myItStop, myStop + 1);

    while (myItStart != myItStop) {
      myRet.push_back(*myItStart);
      ++myItStart;
    }
  });

  return myRet;
}

size_t Lists::remove(const std::string& aName,
                     int64_t            aCount,
                     const std::string& aValue) {
  size_t myRet = 0;

  theApplyer.performOnExisting(aName, [&myRet, aCount, &aValue](List& aList) {
    if (aCount == 0) {
      size_t myRemoved = 0;
      for (auto myIt = aList.begin(); myIt != aList.end();) {
        if (*myIt == aValue) {
          myIt = aList.erase(myIt);
          ++myRemoved;
        } else {
          ++myIt;
        }
      }
      myRet = myRemoved;
    } else {

      auto myToRemove = std::abs(aCount);

      if (aCount > 0) {
        for (auto myIt = aList.begin();
             myIt != aList.end() and myToRemove > 0;) {
          if (*myIt == aValue) {
            myIt = aList.erase(myIt);
            --myToRemove;
          } else {
            ++myIt;
          }
        }
      } else {
        for (auto myIt = aList.rbegin();
             myIt != aList.rend() and myToRemove > 0;) {
          if (*myIt == aValue) {
            myIt = List::reverse_iterator(aList.erase(std::next(myIt).base()));
            --myToRemove;
          } else {
            ++myIt;
          }
        }
      }

      assert(std::abs(aCount) >= myToRemove);
      myRet = std::abs(aCount) - myToRemove;
    }
  });

  return myRet;
}

Lists::Status Lists::set(const std::string& aName,
                         int64_t            aIndex,
                         const std::string& aValue) {

  Status myRet = Status::OK;

  bool myFound = false;

  theApplyer.performOnExisting(aName, [&myRet, &myFound, &aValue, aIndex](List& aList) {
    // If this lambda is called then the list was found
    myFound = true;

    if (aIndex >= 0) {
      if (size_t(aIndex) < aList.size()) {
        auto myIt = aList.begin();
        std::advance(myIt, aIndex);
        *myIt = aValue;
      } else {
        myRet = Status::OUT_OF_RANGE;
      }
    } else {
      auto myReverseIndex = size_t(std::abs(aIndex + 1));
      if (myReverseIndex < aList.size()) {
        auto myIt = aList.rbegin();
        std::advance(myIt, myReverseIndex);
        *myIt = aValue;
      } else {
        myRet = Status::OUT_OF_RANGE;
      }
    }
  });

  return myFound ? myRet : Status::NOT_FOUND;
}

void Lists::trim(const std::string& aName, int64_t aStart, int64_t aStop) {

  theApplyer.performOnExisting(aName, [aStart, aStop](List& aList) {
    if (aList.empty()) {
      return;
    }

    auto myStart = aStart;
    auto myStop  = aStop;

    if (aStart > 0 and size_t(aStart) >= aList.size()) {
      aList.clear();
      return;
    } else if (aStart < 0) {
      if (size_t(-aStart) > aList.size()) {
        myStart = 0;
      } else {
        myStart = aList.size() + aStart;
      }
    }

    if (aStop > 0 and size_t(aStop) >= aList.size()) {
      myStop = aList.size() - 1;
    } else if (aStop < 0) {
      if (size_t(-aStop) > aList.size()) {
        return;
      } else {
        myStop = aList.size() + aStop;
      }
    }

    if (myStart > myStop) {
      aList.clear();
      return;
    }

    if (myStart == 0 and size_t(myStop + 1) == aList.size()) {
      return;
    }

    auto myItStart = aList.begin();
    std::advance(myItStart, myStart);
    auto myItStop = aList.begin();
    std::advance(myItStop, myStop + 1);

    aList.erase(aList.begin(), myItStart);
    aList.erase(myItStop, aList.end());
  });
}

boost::optional<std::string>
Lists::popBackPushFront(const std::string& aSourceName,
                        const std::string& aDestinationName) {

  boost::optional<std::string> myRet;

  // This mutex is required to avoid a dead lock in case two different
  // operations are running:   L1 -> L2
  //                           L2 -> L1
  const boost::lock_guard<PopBackPushFrontMutex> myLock(thePopBackPushFrontMutex);

  theApplyer.performOnExisting(
      aSourceName, [this, &aDestinationName, &myRet](List& aSourceList) {
        if (aSourceList.empty()) {
          return;
        }
        myRet = aSourceList.back();
        aSourceList.pop_back();

        const std::string myValue = myRet.get();

        theApplyer.performOnNew(aDestinationName, [&myValue](List& aDestinationList) {
          aDestinationList.push_front(myValue);
        });
      });

  return myRet;
}

size_t Lists::pushBackExist(const std::string&                   aName,
                            const std::vector<std::string_view>& aValues) {

  size_t myRet = 0;

  theApplyer.performOnExisting(aName, [&myRet, &aValues](List& aList) {
    for (const auto& myString : aValues) {
      aList.push_back(std::string(myString));
    }
    myRet = aList.size();
  });

  return myRet;
}

/////////



} // namespace storage
} // namespace oktoplus
