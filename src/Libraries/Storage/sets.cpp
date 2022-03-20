#include "Storage/sets.h"

namespace okts::stor {

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

} // namespace okts::stor
