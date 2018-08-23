#include "Storage/vectors.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include <boost/thread/lock_guard.hpp>

namespace oktoplus {
namespace storage {

Vectors::Vectors()
    : theApplyer() {
}

size_t Vectors::hostedKeys() const {
  return theApplyer.hostedKeys();
}

size_t Vectors::pushBack(const std::string&                 aName,
                       const std::vector<std::string_view>& aValues) {

  size_t myRet;

  theApplyer.performOnNew(aName, [&myRet, &aValues](Vector& aVector) {
    for (const auto& myString : aValues) {
      aVector.push_back(std::string(myString));
    }
    myRet = aVector.size();
  });

  return myRet;
}

boost::optional<std::string> Vectors::popBack(const std::string& aName) {

  boost::optional<std::string> myRet;

  theApplyer.performOnExisting(aName, [&myRet](Vector& aVector) {
    if (aVector.empty()) {
      return;
    }
    myRet = aVector.back();
    aVector.pop_back();
  });

  return myRet;
}

size_t Vectors::size(const std::string& aName) const {

  size_t myRet = 0;

  theApplyer.performOnExisting(
      aName, [&myRet](const Vector& aVector) { myRet = aVector.size(); });

  return myRet;
}

boost::optional<std::string> Vectors::index(const std::string& aName,
                                            int64_t            aIndex) const {

  boost::optional<std::string> myRet;

  theApplyer.performOnExisting(aName, [&myRet, aIndex](const Vector& aVector) {
    if (aIndex >= 0) {
      if (size_t(aIndex) < aVector.size()) {
        auto myIt = aVector.begin();
        std::advance(myIt, aIndex);
        myRet = *myIt;
      }
    } else {
      auto myReverseIndex = size_t(std::abs(aIndex + 1));
      if (myReverseIndex < aVector.size()) {
        auto myIt = aVector.rbegin();
        std::advance(myIt, myReverseIndex);
        myRet = *myIt;
      }
    }
  });

  return myRet;
}

boost::optional<int64_t> Vectors::insert(const std::string& aName,
                                         Position           aPosition,
                                         const std::string& aPivot,
                                         const std::string& aValue) {
  boost::optional<int64_t> myRet;

  theApplyer.performOnExisting(
      aName, [&myRet, aPosition, &aPivot, &aValue](Vector& aVector) {
        auto myIt = std::find(aVector.begin(), aVector.end(), aPivot);
        if (myIt == aVector.end()) {
          myRet = -1;
          return;
        }

        if (aPosition == Position::AFTER) {
          ++myIt;
        }

        aVector.insert(myIt, aValue);
        myRet = aVector.size();
      });

  return myRet;
}

std::vector<std::string>
Vectors::range(const std::string& aName, int64_t aStart, int64_t aStop) const {

  std::vector<std::string> myRet;

  theApplyer.performOnExisting(
      aName, [&myRet, aStart, aStop](const Vector& aVector) {
        if (aVector.empty()) {
          return;
        }

        auto myStart = aStart;
        auto myStop  = aStop;

        if (aStart > 0 and size_t(aStart) >= aVector.size()) {
          return;
        } else if (aStart < 0) {
          if (size_t(-aStart) > aVector.size()) {
            myStart = 0;
          } else {
            myStart = aVector.size() + aStart;
          }
        }

        if (aStop > 0 and size_t(aStop) >= aVector.size()) {
          myStop = aVector.size() - 1;
        } else if (aStop < 0) {
          if (size_t(-aStop) > aVector.size()) {
            return;
          } else {
            myStop = aVector.size() + aStop;
          }
        }

        if (myStart > myStop) {
          return;
        }

        auto myItStart = aVector.begin();
        std::advance(myItStart, myStart);
        auto myItStop = aVector.begin();
        std::advance(myItStop, myStop + 1);

        while (myItStart != myItStop) {
          myRet.push_back(*myItStart);
          ++myItStart;
        }
      });

  return myRet;
}

size_t Vectors::remove(const std::string& aName,
                     int64_t            aCount,
                     const std::string& aValue) {
  size_t myRet = 0;

  theApplyer.performOnExisting(aName, [&myRet, aCount, &aValue](Vector& aVector) {
    if (aCount == 0) {
      size_t myRemoved = 0;
      for (auto myIt = aVector.begin(); myIt != aVector.end();) {
        if (*myIt == aValue) {
          myIt = aVector.erase(myIt);
          ++myRemoved;
        } else {
          ++myIt;
        }
      }
      myRet = myRemoved;
    } else {

      auto myToRemove = std::abs(aCount);

      if (aCount > 0) {
        for (auto myIt = aVector.begin();
             myIt != aVector.end() and myToRemove > 0;) {
          if (*myIt == aValue) {
            myIt = aVector.erase(myIt);
            --myToRemove;
          } else {
            ++myIt;
          }
        }
      } else {
        for (auto myIt = aVector.rbegin();
             myIt != aVector.rend() and myToRemove > 0;) {
          if (*myIt == aValue) {
            myIt = Vector::reverse_iterator(aVector.erase(std::next(myIt).base()));
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

Vectors::Status Vectors::set(const std::string& aName,
                         int64_t            aIndex,
                         const std::string& aValue) {

  Status myRet = Status::OK;

  bool myFound = false;

  theApplyer.performOnExisting(
      aName, [&myRet, &myFound, &aValue, aIndex](Vector& aVector) {
        // If this lambda is called then the list was found
        myFound = true;

        if (aIndex >= 0) {
          if (size_t(aIndex) < aVector.size()) {
            auto myIt = aVector.begin();
            std::advance(myIt, aIndex);
            *myIt = aValue;
          } else {
            myRet = Status::OUT_OF_RANGE;
          }
        } else {
          auto myReverseIndex = size_t(std::abs(aIndex + 1));
          if (myReverseIndex < aVector.size()) {
            auto myIt = aVector.rbegin();
            std::advance(myIt, myReverseIndex);
            *myIt = aValue;
          } else {
            myRet = Status::OUT_OF_RANGE;
          }
        }
      });

  return myFound ? myRet : Status::NOT_FOUND;
}

void Vectors::trim(const std::string& aName, int64_t aStart, int64_t aStop) {

  theApplyer.performOnExisting(aName, [aStart, aStop](Vector& aVector) {
    if (aVector.empty()) {
      return;
    }

    auto myStart = aStart;
    auto myStop  = aStop;

    if (aStart > 0 and size_t(aStart) >= aVector.size()) {
      aVector.clear();
      return;
    } else if (aStart < 0) {
      if (size_t(-aStart) > aVector.size()) {
        myStart = 0;
      } else {
        myStart = aVector.size() + aStart;
      }
    }

    if (aStop > 0 and size_t(aStop) >= aVector.size()) {
      myStop = aVector.size() - 1;
    } else if (aStop < 0) {
      if (size_t(-aStop) > aVector.size()) {
        return;
      } else {
        myStop = aVector.size() + aStop;
      }
    }

    if (myStart > myStop) {
      aVector.clear();
      return;
    }

    if (myStart == 0 and size_t(myStop + 1) == aVector.size()) {
      return;
    }

    auto myItStart = aVector.begin();
    std::advance(myItStart, myStart);
    auto myItStop = aVector.begin();
    std::advance(myItStop, myStop + 1);

    aVector.erase(aVector.begin(), myItStart);
    aVector.erase(myItStop, aVector.end());
  });
}

size_t Vectors::pushBackExist(const std::string&                   aName,
                            const std::vector<std::string_view>& aValues) {

  size_t myRet = 0;

  theApplyer.performOnExisting(aName, [&myRet, &aValues](Vector& aVector) {
    for (const auto& myString : aValues) {
      aVector.push_back(std::string(myString));
    }
    myRet = aVector.size();
  });

  return myRet;
}

/////////

} // namespace storage
} // namespace oktoplus
