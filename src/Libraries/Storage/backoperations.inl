
template <class CONTAINER>
size_t BackOperations<CONTAINER>::pushBack(const std::string&                 aName,
                                           const std::vector<std::string_view>& aValues) {

  size_t myRet;

  Base::theApplyer.performOnNew(aName, [&myRet, &aValues](Container& aContainer) {
    for (const auto& myString : aValues) {
      aContainer.push_back(std::string(myString));
    }
    myRet = aContainer.size();
  });

  return myRet;
}

template <class CONTAINER>
boost::optional<std::string> BackOperations<CONTAINER>::popBack(const std::string& aName) {

  boost::optional<std::string> myRet;

  Base::theApplyer.performOnExisting(aName, [&myRet](Container& aContainer) {
    if (aContainer.empty()) {
      return;
    }
    myRet = aContainer.back();
    aContainer.pop_back();
  });

  return myRet;
}


template <class CONTAINER>
size_t BackOperations<CONTAINER>::size(const std::string& aName) const {

  size_t myRet = 0;

  Base::theApplyer.performOnExisting(
      aName, [&myRet](const Container& aContainer) { myRet = aContainer.size(); });

  return myRet;
}

template <class CONTAINER>
boost::optional<std::string> BackOperations<CONTAINER>::index(const std::string& aName,
                                                              int64_t            aIndex) const {

  boost::optional<std::string> myRet;

  Base::theApplyer.performOnExisting(aName, [&myRet, aIndex](const Container& aContainer) {
    if (aIndex >= 0) {
      if (size_t(aIndex) < aContainer.size()) {
        auto myIt = aContainer.begin();
        std::advance(myIt, aIndex);
        myRet = *myIt;
      }
    } else {
      auto myReverseIndex = size_t(std::abs(aIndex + 1));
      if (myReverseIndex < aContainer.size()) {
        auto myIt = aContainer.rbegin();
        std::advance(myIt, myReverseIndex);
        myRet = *myIt;
      }
    }
  });

  return myRet;
}

template <class CONTAINER>
boost::optional<int64_t> BackOperations<CONTAINER>::insert(const std::string& aName,
                                         Position           aPosition,
                                         const std::string& aPivot,
                                         const std::string& aValue) {
  boost::optional<int64_t> myRet;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, aPosition, &aPivot, &aValue](Container& aContainer) {
        auto myIt = std::find(aContainer.begin(), aContainer.end(), aPivot);
        if (myIt == aContainer.end()) {
          myRet = -1;
          return;
        }

        if (aPosition == Position::AFTER) {
          ++myIt;
        }

        aContainer.insert(myIt, aValue);
        myRet = aContainer.size();
      });

  return myRet;
}

template <class CONTAINER>
std::vector<std::string>
BackOperations<CONTAINER>::range(const std::string& aName, int64_t aStart, int64_t aStop) const {

  std::vector<std::string> myRet;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, aStart, aStop](const Container& aContainer) {
        if (aContainer.empty()) {
          return;
        }

        auto myStart = aStart;
        auto myStop  = aStop;

        if (aStart > 0 and size_t(aStart) >= aContainer.size()) {
          return;
        } else if (aStart < 0) {
          if (size_t(-aStart) > aContainer.size()) {
            myStart = 0;
          } else {
            myStart = aContainer.size() + aStart;
          }
        }

        if (aStop > 0 and size_t(aStop) >= aContainer.size()) {
          myStop = aContainer.size() - 1;
        } else if (aStop < 0) {
          if (size_t(-aStop) > aContainer.size()) {
            return;
          } else {
            myStop = aContainer.size() + aStop;
          }
        }

        if (myStart > myStop) {
          return;
        }

        auto myItStart = aContainer.begin();
        std::advance(myItStart, myStart);
        auto myItStop = aContainer.begin();
        std::advance(myItStop, myStop + 1);

        while (myItStart != myItStop) {
          myRet.push_back(*myItStart);
          ++myItStart;
        }
      });

  return myRet;
}

template <class CONTAINER>
size_t BackOperations<CONTAINER>::remove(const std::string& aName,
                     int64_t            aCount,
                     const std::string& aValue) {
  size_t myRet = 0;

  Base::theApplyer.performOnExisting(aName, [&myRet, aCount, &aValue](Container& aContainer) {
    if (aCount == 0) {
      size_t myRemoved = 0;
      for (auto myIt = aContainer.begin(); myIt != aContainer.end();) {
        if (*myIt == aValue) {
          myIt = aContainer.erase(myIt);
          ++myRemoved;
        } else {
          ++myIt;
        }
      }
      myRet = myRemoved;
    } else {

      auto myToRemove = std::abs(aCount);

      if (aCount > 0) {
        for (auto myIt = aContainer.begin();
             myIt != aContainer.end() and myToRemove > 0;) {
          if (*myIt == aValue) {
            myIt = aContainer.erase(myIt);
            --myToRemove;
          } else {
            ++myIt;
          }
        }
      } else {
        for (auto myIt = aContainer.rbegin();
             myIt != aContainer.rend() and myToRemove > 0;) {
          if (*myIt == aValue) {
            myIt = typename Container::reverse_iterator(aContainer.erase(std::next(myIt).base()));
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

template <class CONTAINER>
typename BackOperations<CONTAINER>::Status BackOperations<CONTAINER>::set(const std::string& aName,
                         int64_t            aIndex,
                         const std::string& aValue) {

  Status myRet = Status::OK;

  bool myFound = false;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, &myFound, &aValue, aIndex](Container& aContainer) {
        // If this lambda is called then the list was found
        myFound = true;

        if (aIndex >= 0) {
          if (size_t(aIndex) < aContainer.size()) {
            auto myIt = aContainer.begin();
            std::advance(myIt, aIndex);
            *myIt = aValue;
          } else {
            myRet = Status::OUT_OF_RANGE;
          }
        } else {
          auto myReverseIndex = size_t(std::abs(aIndex + 1));
          if (myReverseIndex < aContainer.size()) {
            auto myIt = aContainer.rbegin();
            std::advance(myIt, myReverseIndex);
            *myIt = aValue;
          } else {
            myRet = Status::OUT_OF_RANGE;
          }
        }
      });

  return myFound ? myRet : Status::NOT_FOUND;
}

template <class CONTAINER>
void BackOperations<CONTAINER>::trim(const std::string& aName, int64_t aStart, int64_t aStop) {

  Base::theApplyer.performOnExisting(aName, [aStart, aStop](Container& aContainer) {
    if (aContainer.empty()) {
      return;
    }

    auto myStart = aStart;
    auto myStop  = aStop;

    if (aStart > 0 and size_t(aStart) >= aContainer.size()) {
      aContainer.clear();
      return;
    } else if (aStart < 0) {
      if (size_t(-aStart) > aContainer.size()) {
        myStart = 0;
      } else {
        myStart = aContainer.size() + aStart;
      }
    }

    if (aStop > 0 and size_t(aStop) >= aContainer.size()) {
      myStop = aContainer.size() - 1;
    } else if (aStop < 0) {
      if (size_t(-aStop) > aContainer.size()) {
        return;
      } else {
        myStop = aContainer.size() + aStop;
      }
    }

    if (myStart > myStop) {
      aContainer.clear();
      return;
    }

    if (myStart == 0 and size_t(myStop + 1) == aContainer.size()) {
      return;
    }

    auto myItStart = aContainer.begin();
    std::advance(myItStart, myStart);
    auto myItStop = aContainer.begin();
    std::advance(myItStop, myStop + 1);

    aContainer.erase(aContainer.begin(), myItStart);
    aContainer.erase(myItStop, aContainer.end());
  });
}

template <class CONTAINER>
size_t BackOperations<CONTAINER>::pushBackExist(const std::string&                   aName,
                            const std::vector<std::string_view>& aValues) {

  size_t myRet = 0;

  Base::theApplyer.performOnExisting(aName, [&myRet, &aValues](Container& aContainer) {
    for (const auto& myString : aValues) {
      aContainer.push_back(std::string(myString));
    }
    myRet = aContainer.size();
  });

  return myRet;
}
