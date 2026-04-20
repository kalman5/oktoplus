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

  size_t myAdded = 0;

  theApplyer.performOnNew(aName, [&myAdded, &aValues](Container& aContainer) {
    for (const auto& myString : aValues) {
      if (aContainer.insert(std::string(myString)).second) {
        ++myAdded;
      }
    }
  });

  return myAdded;
}

size_t Sets::cardinality(const std::string& aName) const {

  size_t myRet = 0;

  theApplyer.performOnExisting(aName, [&myRet](const Container& aContainer) {
    myRet = aContainer.size();
  });

  return myRet;
}

Sets::Container
Sets::diff(const std::vector<std::string_view>& aNames) {

  Sets::Container myRet;

  if (aNames.empty()) {
    return myRet;
  }

  theApplyer.performOnExisting(
      std::string(aNames[0]),
      [&myRet](const Container& aContainer) { myRet = aContainer; });

  for (size_t i = 1; i < aNames.size(); ++i) {

    theApplyer.performOnExisting(
        std::string(aNames[i]), [&myRet](const Container& aContainer) {
          absl::erase_if(myRet, [&aContainer](const std::string& aVal) {
            return aContainer.count(aVal) > 0;
          });
        });
  }

  return myRet;
}

size_t Sets::diffStore(const std::string&                   aDestination,
                       const std::vector<std::string_view>& aNames) {
  auto myResult = diff(aNames);
  auto mySize   = myResult.size();

  if (myResult.empty()) {
    theApplyer.performOnExisting(
        aDestination, [](Container& aContainer) { aContainer.clear(); });
  } else {
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
          absl::erase_if(myRet, [&aContainer](const std::string& aVal) {
            return aContainer.count(aVal) == 0;
          });
        });
  }

  return myRet;
}

size_t Sets::interStore(const std::string&                   aDestination,
                        const std::vector<std::string_view>& aNames) {
  auto myResult = inter(aNames);
  auto mySize   = myResult.size();

  if (myResult.empty()) {
    theApplyer.performOnExisting(
        aDestination, [](Container& aContainer) { aContainer.clear(); });
  } else {
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
  // SMOVE must be atomic w.r.t. other clients: a third party must
  // never see the value missing from both sets, nor present in both.
  //
  // Same-key special case: SMOVE k k v is a no-op that returns 1 if v
  // is present, 0 otherwise. We can't nest performOnNew(k) inside
  // performOnExisting(k) because the inner per-key mutex is non-
  // recursive — re-acquiring it would deadlock. Probe under one lock.
  if (aSource == aDestination) {
    bool myPresent = false;
    theApplyer.performOnExisting(
        aSource, [&myPresent, &aValue](const Container& aContainer) {
          myPresent = aContainer.count(aValue) > 0;
        });
    return myPresent;
  }

  // Cross-key path. The whole erase-from-source + insert-into-
  // destination must run with BOTH per-key inner mutexes held
  // simultaneously to be observable as a single step. We achieve that
  // by nesting performOnNew(destination, ...) inside the
  // performOnExisting(source, ...) lambda — once the inner lambda
  // runs, both inner mutexes are held.
  //
  // theMoveMutex serialises every cross-key SMOVE so that two
  // concurrent SMOVEs in opposite directions can't deadlock-spin
  // (A holds src.inner waiting for dst.inner; B holds dst.inner
  // waiting for src.inner). Same pattern as
  // SequenceContainer::move's theMoveMutex; SMOVE isn't a hot
  // command, so the global serialisation cost is acceptable.
  const std::lock_guard<std::mutex> myLock(theMoveMutex);

  bool myMoved = false;
  theApplyer.performOnExisting(
      aSource,
      [this, &aDestination, &aValue, &myMoved](Container& aSourceContainer) {
        // Source key existed (lambda only runs if so). Erase first;
        // if the value wasn't there, no destination mutation either,
        // so we leave myMoved=false and return 0 to the client.
        if (aSourceContainer.erase(aValue) == 0) {
          return;
        }
        myMoved = true;

        // Insert into destination. Sets are idempotent, so a
        // pre-existing value in destination is fine. performOnNew
        // creates the destination key if it doesn't yet exist.
        theApplyer.performOnNew(
            aDestination, [&aValue](Container& aDestContainer) {
              aDestContainer.insert(aValue);
            });
      });

  return myMoved;
}

std::vector<std::string> Sets::pop(const std::string& aName, size_t aCount) {
  std::vector<std::string> myRet;

  theApplyer.performOnExisting(
      aName, [&myRet, aCount](Container& aContainer) {
        size_t myToPop = std::min(aCount, aContainer.size());
        myRet.reserve(myToPop);

        // std::sample picks myToPop items without replacement in a
        // single forward-iterator pass — O(N), N space. The previous
        // approach was O(K*N): each iteration constructed a new
        // distribution and std::advance'd from begin (O(N) on
        // flat_hash_set's forward iterators) → unusable on large sets.
        std::sample(aContainer.begin(), aContainer.end(),
                    std::back_inserter(myRet), myToPop, rng());

        // Erase the sampled values by hash lookup (O(K) average).
        for (const auto& myVal : myRet) {
          aContainer.erase(myVal);
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
          // Without replacement: std::sample is single-pass, K-bounded
          // working set. The previous shuffle copied the whole set
          // even when K << N.
          auto myN = std::min(static_cast<size_t>(aCount), aContainer.size());
          myRet.reserve(myN);
          std::sample(aContainer.begin(), aContainer.end(),
                      std::back_inserter(myRet), myN, rng());
        } else {
          // With replacement: we genuinely need an indexable view to
          // pick K times. Materialise once (O(N)) then K random picks
          // (O(K)) — total O(N + K), better than O(K*N) of advancing
          // a forward iterator per pick on flat_hash_set.
          auto                     myN = static_cast<size_t>(-aCount);
          std::vector<std::string> myAll(aContainer.begin(),
                                         aContainer.end());
          myRet.reserve(myN);
          auto myDist =
              std::uniform_int_distribution<size_t>(0, myAll.size() - 1);
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

  if (myResult.empty()) {
    theApplyer.performOnExisting(
        aDestination, [](Container& aContainer) { aContainer.clear(); });
  } else {
    theApplyer.performOnNew(
        aDestination,
        [&myResult](Container& aContainer) { aContainer = std::move(myResult); });
  }

  return mySize;
}

} // namespace okts::stor
