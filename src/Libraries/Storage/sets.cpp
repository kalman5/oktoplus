#include "Storage/sets.h"

namespace okts {
namespace stor {

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

size_t Sets::cardinality(const std::string& aName) {

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

  const std::string myFirstOperand(aNames[0].begin(), aNames[0].end());
  theApplyer.performOnExisting(
      myFirstOperand,
      [&myRet](const Container& aContainer) { myRet = aContainer; });

  for (size_t i = 1; i < aNames.size(); ++i) {

    const std::string myOperand(aNames[i].begin(), aNames[i].end());

    theApplyer.performOnExisting(
        myOperand, [&myRet](const Container& aContainer) {
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

} // namespace stor
} // namespace okts
