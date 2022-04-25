#pragma once

#include "Storage/genericcontainer.h"

#include "Support/noncopyable.h"

#include <list>
#include <optional>
#include <string>
#include <string_view>

namespace okts {
namespace stor {

template <class CONTAINER>
class BackOperations : virtual public GenericContainer<CONTAINER>
{
  using Container = CONTAINER;
  using Base      = GenericContainer<Container>;

 public:
  using Status    = typename Base::Status;
  using Position  = typename Base::Position;
  using Direction = typename Base::Direction;

  DISABLE_EVIL_CONSTRUCTOR(BackOperations);

  BackOperations()
      : Base() {
  }

  size_t pushBack(const std::string&                   aName,
                  const std::vector<std::string_view>& aValues);

  std::list<std::string> popBack(const std::string& aName, uint64_t aCount);

  size_t size(const std::string& aName) const;

  std::optional<std::string> index(const std::string& aName,
                                   int64_t            aIndex) const;

  std::optional<int64_t> insert(const std::string& aName,
                                Position           aPosition,
                                const std::string& aPivot,
                                const std::string& aValue);

  std::optional<std::string> move(const std::string& aSourceName,
                                  const std::string& aDestinationName,
                                  Direction          aSourceDirection,
                                  Direction          aDestinationDirection);

  std::list<uint64_t> position(const std::string& aName,
                               const std::string& aValue,
                               int64_t            aRank,
                               uint64_t           aCount,
                               uint64_t           aMaxLength);

  std::list<std::string>
  range(const std::string& aName, int64_t aStart, int64_t aStop) const;

  size_t
  remove(const std::string& aName, int64_t aCount, const std::string& aValue);

  Status
  set(const std::string& aName, int64_t aIndex, const std::string& aValue);

  void trim(const std::string& aName, int64_t aStart, int64_t aStop);

  size_t pushBackExist(const std::string&                   aName,
                       const std::vector<std::string_view>& aValues);

 private:
  using MoveMutex = boost::mutex;
  MoveMutex theMoveMutex;
};

//// INLINE DEFINITIONS

template <class CONTAINER>
size_t BackOperations<CONTAINER>::pushBack(
    const std::string& aName, const std::vector<std::string_view>& aValues) {

  size_t myRet;

  Base::theApplyer.performOnNew(aName,
                                [&myRet, &aValues](Container& aContainer) {
                                  for (const auto& myString : aValues) {
                                    aContainer.push_back(std::string(myString));
                                  }
                                  myRet = aContainer.size();
                                });

  return myRet;
}

template <class CONTAINER>
std::list<std::string>
BackOperations<CONTAINER>::popBack(const std::string& aName,
                                   const uint64_t     aCount) {

  std::list<std::string> myRet;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, &aCount](Container& aContainer) {
        uint64_t myCollected = 0;
        while (!aContainer.empty() && myCollected < aCount) {
          myRet.emplace_back(std::move(aContainer.back()));
          aContainer.pop_back();
          ++myCollected;
        }
      });

  return myRet;
}

template <class CONTAINER>
size_t BackOperations<CONTAINER>::size(const std::string& aName) const {

  size_t myRet = 0;

  Base::theApplyer.performOnExisting(
      aName,
      [&myRet](const Container& aContainer) { myRet = aContainer.size(); });

  return myRet;
}

template <class CONTAINER>
std::optional<std::string>
BackOperations<CONTAINER>::index(const std::string& aName,
                                 int64_t            aIndex) const {

  std::optional<std::string> myRet;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, aIndex](const Container& aContainer) {
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
std::optional<int64_t>
BackOperations<CONTAINER>::insert(const std::string& aName,
                                  Position           aPosition,
                                  const std::string& aPivot,
                                  const std::string& aValue) {
  std::optional<int64_t> myRet;

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
std::optional<std::string>
BackOperations<CONTAINER>::move(const std::string& aSourceName,
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

  const boost::lock_guard myLock(theMoveMutex);

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

template <class CONTAINER>
std::list<uint64_t>
BackOperations<CONTAINER>::position(const std::string& aName,
                                    const std::string& aValue,
                                    const int64_t      aRank,
                                    const uint64_t     aCount,
                                    const uint64_t     aMaxLength) {
  std::list<uint64_t> myRet;

  Base::theApplyer.performOnExisting(
      aName,
      [&myRet, &aValue, &aRank, &aCount, &aMaxLength](
          const Container& aContainer) {
        uint64_t   myIndex = 0;
        uint64_t   myFound = 0;
        auto const myMaxLength =
            aMaxLength == 0 ? std::numeric_limits<uint64_t>::max() : aMaxLength;
        const uint64_t myURank = aRank > 0 ? aRank : std::abs(aRank);

        if (aRank > 0) {
          for (auto myIt = aContainer.begin(); myIt != aContainer.end();
               ++myIt) {
            if (*myIt == aValue) {
              ++myFound;
              if (myFound < myURank) {
                ++myIndex;
                continue;
              }
              myRet.push_back(myIndex);
            }

            ++myIndex;

            // At max we can return the amount of aCount indexes.
            if (myRet.size() == aCount) {
              break;
            }

            /// Interrupt the traverse if I have visited already the max
            /// allowed values.
            if (myIndex >= myMaxLength) {
              break;
            }
          }
        } else {
          for (auto myIt = aContainer.rbegin(); myIt != aContainer.rend();
               ++myIt) {
            if (*myIt == aValue) {
              ++myFound;
              if (myFound < myURank) {
                ++myIndex;
                continue;
              }
              // Returned index is always 0 based traversing from left.
              myRet.push_back(aContainer.size() - 1 - myIndex);
            }

            ++myIndex;

            // At max we can return the amount of aCount indexes.
            if (myRet.size() == aCount) {
              break;
            }

            /// Interrupt the traverse if I have visited already the max
            /// allowed values.
            if (myIndex >= myMaxLength) {
              break;
            }
          }
        }
      });

  return myRet;
}

template <class CONTAINER>
std::list<std::string> BackOperations<CONTAINER>::range(
    const std::string& aName, int64_t aStart, int64_t aStop) const {

  std::list<std::string> myRet;

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
          myRet.emplace_back(*myItStart);
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

  Base::theApplyer.performOnExisting(
      aName, [&myRet, aCount, &aValue](Container& aContainer) {
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
                myIt = typename Container::reverse_iterator(
                    aContainer.erase(std::next(myIt).base()));
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
typename BackOperations<CONTAINER>::Status BackOperations<CONTAINER>::set(
    const std::string& aName, int64_t aIndex, const std::string& aValue) {

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
void BackOperations<CONTAINER>::trim(const std::string& aName,
                                     int64_t            aStart,
                                     int64_t            aStop) {
  Base::theApplyer.performOnExisting(
      aName, [aStart, aStop](Container& aContainer) {
        if (aContainer.empty()) {
          return;
        }

        auto myStart = aStart;
        auto myStop  = aStop;
        /*
                if (myStart > myStop) {
                  aContainer.clear();
                  return;
                }
        */
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
size_t BackOperations<CONTAINER>::pushBackExist(
    const std::string& aName, const std::vector<std::string_view>& aValues) {

  size_t myRet = 0;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, &aValues](Container& aContainer) {
        for (const auto& myString : aValues) {
          aContainer.push_back(std::string(myString));
        }
        myRet = aContainer.size();
      });

  return myRet;
}

} // namespace stor
} // namespace okts
