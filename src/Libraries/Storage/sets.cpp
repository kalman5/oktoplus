#include "Storage/sets.h"

#include <algorithm>
#include <random>

namespace okts::stor {

namespace {
std::mt19937& rng() {
  static thread_local std::mt19937 myGen(std::random_device{}());
  return myGen;
}
} // namespace

Sets::Sets()
    : Base() {
}

size_t Sets::add(const std::string&                   aName,
                 const std::vector<std::string_view>& aValues) {

  size_t myRet;

  theApplyer.performOnNew(aName, [&myRet, &aValues](Container& aContainer) {
    for (const auto& myString : aValues) {
      aContainer.insert(std::string(myString));
    }
    myRet = aContainer.size();
  });

  return myRet;
}

size_t Sets::cardinality(const std::string& aName) const {

  size_t myRet = 0;

  theApplyer.performOnExisting(aName, [&myRet](const Container& aContainer) {
    myRet = aContainer.size();
  });

  return myRet;
}

std::unordered_set<std::string>
Sets::diff(const std::vector<std::string_view>& aNames) {

  std::unordered_set<std::string> myRet;

  if (aNames.empty()) {
    return myRet;
  }

  theApplyer.performOnExisting(
      std::string(aNames[0]),
      [&myRet](const Container& aContainer) { myRet = aContainer; });

  for (size_t i = 1; i < aNames.size(); ++i) {

    theApplyer.performOnExisting(
        std::string(aNames[i]), [&myRet](const Container& aContainer) {
          for (auto it = myRet.begin(); it != myRet.end(); /*void*/) {
            if (aContainer.count(*it)) {
              it = myRet.erase(it);
            } else {
              ++it;
            }
          }
        });
  }

  return myRet;
}

size_t Sets::diffStore(const std::string&                   aDestination,
                       const std::vector<std::string_view>& aNames) {
  auto myResult = diff(aNames);
  auto mySize   = myResult.size();

  if (!myResult.empty()) {
    theApplyer.performOnNew(
        aDestination,
        [&myResult](Container& aContainer) { aContainer = std::move(myResult); });
  }

  return mySize;
}

Sets::Container Sets::inter(const std::vector<std::string_view>& aNames) {
  Container myRet;

  if (aNames.empty()) {
    return myRet;
  }

  theApplyer.performOnExisting(
      std::string(aNames[0]),
      [&myRet](const Container& aContainer) { myRet = aContainer; });

  for (size_t i = 1; i < aNames.size() && !myRet.empty(); ++i) {
    theApplyer.performOnExisting(
        std::string(aNames[i]), [&myRet](const Container& aContainer) {
          for (auto it = myRet.begin(); it != myRet.end();) {
            if (aContainer.count(*it) == 0) {
              it = myRet.erase(it);
            } else {
              ++it;
            }
          }
        });
  }

  return myRet;
}

size_t Sets::interStore(const std::string&                   aDestination,
                        const std::vector<std::string_view>& aNames) {
  auto myResult = inter(aNames);
  auto mySize   = myResult.size();

  if (!myResult.empty()) {
    theApplyer.performOnNew(
        aDestination,
        [&myResult](Container& aContainer) { aContainer = std::move(myResult); });
  }

  return mySize;
}

bool Sets::isMember(const std::string& aName,
                    const std::string& aValue) const {
  bool myRet = false;

  theApplyer.performOnExisting(
      aName, [&myRet, &aValue](const Container& aContainer) {
        myRet = aContainer.count(aValue) > 0;
      });

  return myRet;
}

std::vector<bool>
Sets::misMember(const std::string&              aName,
                const std::vector<std::string>& aValues) const {
  std::vector<bool> myRet(aValues.size(), false);

  theApplyer.performOnExisting(
      aName, [&myRet, &aValues](const Container& aContainer) {
        for (size_t i = 0; i < aValues.size(); ++i) {
          myRet[i] = aContainer.count(aValues[i]) > 0;
        }
      });

  return myRet;
}

Sets::Container Sets::members(const std::string& aName) const {
  Container myRet;

  theApplyer.performOnExisting(
      aName,
      [&myRet](const Container& aContainer) { myRet = aContainer; });

  return myRet;
}

bool Sets::moveMember(const std::string& aSource,
                      const std::string& aDestination,
                      const std::string& aValue) {
  bool myRemoved = false;

  theApplyer.performOnExisting(
      aSource, [&myRemoved, &aValue](Container& aContainer) {
        myRemoved = aContainer.erase(aValue) > 0;
      });

  if (myRemoved) {
    theApplyer.performOnNew(
        aDestination, [&aValue](Container& aContainer) {
          aContainer.insert(aValue);
        });
  }

  return myRemoved;
}

std::vector<std::string> Sets::pop(const std::string& aName, size_t aCount) {
  std::vector<std::string> myRet;

  theApplyer.performOnExisting(
      aName, [&myRet, aCount](Container& aContainer) {
        size_t myToPop = std::min(aCount, aContainer.size());
        myRet.reserve(myToPop);

        for (size_t i = 0; i < myToPop; ++i) {
          auto myDist = std::uniform_int_distribution<size_t>(
              0, aContainer.size() - 1);
          auto myIt = aContainer.begin();
          std::advance(myIt, myDist(rng()));
          myRet.push_back(std::move(*myIt));
          aContainer.erase(myIt);
        }
      });

  return myRet;
}

std::vector<std::string>
Sets::randMember(const std::string& aName, int64_t aCount) const {
  std::vector<std::string> myRet;

  theApplyer.performOnExisting(
      aName, [&myRet, aCount](const Container& aContainer) {
        if (aContainer.empty()) {
          return;
        }

        if (aCount >= 0) {
          auto myN = std::min(static_cast<size_t>(aCount), aContainer.size());
          std::vector<std::string> myAll(aContainer.begin(), aContainer.end());
          std::shuffle(myAll.begin(), myAll.end(), rng());
          myRet.assign(myAll.begin(), myAll.begin() + myN);
        } else {
          auto myN = static_cast<size_t>(-aCount);
          std::vector<std::string> myAll(aContainer.begin(), aContainer.end());
          myRet.reserve(myN);
          auto myDist = std::uniform_int_distribution<size_t>(0, myAll.size() - 1);
          for (size_t i = 0; i < myN; ++i) {
            myRet.push_back(myAll[myDist(rng())]);
          }
        }
      });

  return myRet;
}

size_t Sets::remove(const std::string&                   aName,
                    const std::vector<std::string_view>& aValues) {
  size_t myRet = 0;

  theApplyer.performOnExisting(
      aName, [&myRet, &aValues](Container& aContainer) {
        for (const auto& myValue : aValues) {
          myRet += aContainer.erase(std::string(myValue));
        }
      });

  return myRet;
}

Sets::Container Sets::unionSets(const std::vector<std::string_view>& aNames) {
  Container myRet;

  for (const auto& myName : aNames) {
    theApplyer.performOnExisting(
        std::string(myName), [&myRet](const Container& aContainer) {
          myRet.insert(aContainer.begin(), aContainer.end());
        });
  }

  return myRet;
}

size_t Sets::unionStore(const std::string&                   aDestination,
                        const std::vector<std::string_view>& aNames) {
  auto myResult = unionSets(aNames);
  auto mySize   = myResult.size();

  if (!myResult.empty()) {
    theApplyer.performOnNew(
        aDestination,
        [&myResult](Container& aContainer) { aContainer = std::move(myResult); });
  }

  return mySize;
}

} // namespace okts::stor
