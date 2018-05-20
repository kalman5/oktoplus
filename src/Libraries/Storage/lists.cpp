#include "Storage/lists.h"

#include <algorithm>
#include <string>

namespace oktoplus {
namespace storage {

Lists::Lists()
    : theMutex()
    , theStorage() {
}

size_t Lists::pushBack(const std::string&                   aName,
                       const std::vector<std::string_view>& aValues) {

  size_t myRet;

  performOnNew(aName, [&myRet, &aValues](List& aList) {
    for (const auto& myString : aValues) {
      aList.push_back(std::string(myString));
    }
    myRet = aList.size();
  });

  return myRet;
}

size_t Lists::pushFront(const std::string&                   aName,
                        const std::vector<std::string_view>& aValues) {

  size_t myRet;

  performOnNew(aName, [&myRet, &aValues](List& aList) {
    for (const auto& myString : aValues) {
      aList.push_front(std::string(myString));
    }
    myRet = aList.size();
  });

  return myRet;
}

boost::optional<std::string> Lists::popBack(const std::string& aName) {

  boost::optional<std::string> myRet;

  performOnExisting(aName, [&myRet](List& aList) {
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

  performOnExisting(aName, [&myRet](List& aList) {
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

  performOnExisting(
      aName, [&myRet](const List& aList) { myRet = aList.size(); });

  return myRet;
}

boost::optional<std::string> Lists::index(const std::string& aName,
                                          int64_t            aIndex) const {

  boost::optional<std::string> myRet;

  performOnExisting(aName, [&myRet, aIndex](const List& aList) {
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

  performOnExisting(
      aName, [&myRet, aPosition, &aPivot, &aValue](List& aList) {
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

/////////

void Lists::performOnNew(const std::string& aName, const Functor& aFunctor) {

  ProtectedList* myList = nullptr;

  {
    boost::lock_guard<boost::mutex> myLock(theMutex);
    myList = &theStorage[aName];
  }

  boost::lock_guard<boost::mutex> myLock(myList->mutex);
  aFunctor(myList->list);
}

void Lists::performOnExisting(const std::string& aName,
                              const Functor&     aFunctor) {

  ProtectedList* myList = nullptr;

  {
    boost::lock_guard<boost::mutex> myLock(theMutex);
    auto myIt = theStorage.find(aName);
    if (myIt == theStorage.end()) {
      return;
    }
    myList = &myIt->second;
  }

  boost::lock_guard<boost::mutex> myLock(myList->mutex);
  aFunctor(myList->list);
}

void Lists::performOnExisting(const std::string&  aName,
                              const ConstFunctor& aFunctor) const {

  const ProtectedList* myList = nullptr;

  {
    boost::lock_guard<boost::mutex> myLock(theMutex);
    auto myIt = theStorage.find(aName);
    if (myIt == theStorage.end()) {
      return;
    }
    myList = &myIt->second;
  }

  boost::lock_guard<boost::mutex> myLock(myList->mutex);
  aFunctor(myList->list);
}

} // namespace storage
} // namespace oktoplus
