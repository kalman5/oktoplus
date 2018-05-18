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

  performOnNew(aName, [&myRet, &aValues](ProtectedList& aList) {
    for (const auto& myString : aValues) {
      aList.second.push_back(std::string(myString));
    }
    myRet = aList.second.size();
  });

  return myRet;
}

size_t Lists::pushFront(const std::string&                   aName,
                        const std::vector<std::string_view>& aValues) {

  size_t myRet;

  performOnNew(aName, [&myRet, &aValues](ProtectedList& aList) {
    for (const auto& myString : aValues) {
      aList.second.push_front(std::string(myString));
    }
    myRet = aList.second.size();
  });

  return myRet;
}

boost::optional<std::string> Lists::popBack(const std::string& aName) {

  boost::optional<std::string> myRet;

  performOnExisting(aName, [&myRet](ProtectedList& aList) {
    myRet = aList.second.back();
    aList.second.pop_back();
  });

  return myRet;
}

boost::optional<std::string> Lists::popFront(const std::string& aName) {

  boost::optional<std::string> myRet;

  performOnExisting(aName, [&myRet](ProtectedList& aList) {
    myRet = aList.second.front();
    aList.second.pop_front();
  });

  return myRet;
}

boost::optional<size_t> Lists::size(const std::string& aName) {

  boost::optional<size_t> myRet;

  performOnExisting(
      aName, [&myRet](ProtectedList& aList) { myRet = aList.second.size(); });

  return myRet;
}

boost::optional<std::string> Lists::index(const std::string& aName,
                                          int64_t            aIndex) {

  boost::optional<std::string> myRet;

  performOnExisting(aName, [&myRet, aIndex](ProtectedList& aList) {
    if (aIndex >= 0) {
      if (size_t(aIndex) < aList.second.size()) {
        auto myIt = aList.second.begin();
        std::advance(myIt, aIndex);
        myRet = *myIt;
      }
    } else {
      auto myReverseIndex = size_t(std::abs(aIndex + 1));
      if (myReverseIndex < aList.second.size()) {
        auto myIt = aList.second.rbegin();
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
      aName, [&myRet, aPosition, &aPivot, &aValue](ProtectedList& aList) {
        auto myIt = std::find(aList.second.begin(), aList.second.end(), aPivot);
        if (myIt == aList.second.end()) {
          myRet = -1;
          return;
        }

        if (aPosition == Position::AFTER) {
          ++myIt;
        }

        aList.second.insert(myIt, aValue);
        myRet = aList.second.size();
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

  boost::lock_guard<boost::mutex> myLock(myList->first);

  aFunctor(*myList);
}

void Lists::performOnExisting(const std::string& aName,
                              const Functor&     aFunctor) {

  Storage::iterator myIt;

  {
    boost::lock_guard<boost::mutex> myLock(theMutex);
    myIt = theStorage.find(aName);
  }

  if (myIt != theStorage.end()) {
    ProtectedList&                  myList = myIt->second;
    boost::lock_guard<boost::mutex> myLock(myList.first);

    aFunctor(myList);
  }
}

} // namespace storage
} // namespace oktoplus
