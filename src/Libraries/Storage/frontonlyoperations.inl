
template <class CONTAINER>
size_t FrontOnlyOperations<CONTAINER>::pushFront(
    const std::string_view&              aName,
    const std::vector<std::string_view>& aValues) {

  size_t myRet;

  Base::theApplyer.performOnNew(
      aName, [&myRet, &aValues](Container& aContainer) {
        for (const auto& myString : aValues) {
          aContainer.push_front(std::string(myString));
        }
        myRet = aContainer.size();
      });

  return myRet;
}

template <class CONTAINER>
std::optional<std::string>
FrontOnlyOperations<CONTAINER>::popFront(const std::string_view& aName) {

  std::optional<std::string> myRet;

  Base::theApplyer.performOnExisting(aName, [&myRet](Container& aContainer) {
    if (aContainer.empty()) {
      return;
    }
    myRet = aContainer.front();
    aContainer.pop_front();
  });

  return myRet;
}

template <class CONTAINER>
size_t FrontOnlyOperations<CONTAINER>::pushFrontExist(
    const std::string_view&              aName,
    const std::vector<std::string_view>& aValues) {

  size_t myRet = 0;

  Base::theApplyer.performOnExisting(
      aName, [&myRet, &aValues](Container& aContainer) {
        for (const auto& myString : aValues) {
          aContainer.push_front(std::string(myString));
        }
        myRet = aContainer.size();
      });

  return myRet;
}

template <class CONTAINER>
std::optional<std::string> FrontOnlyOperations<CONTAINER>::popBackPushFront(
    const std::string_view& aSourceName,
    const std::string_view& aDestinationName) {

  std::optional<std::string> myRet;

  // clang-format off
  // This mutex is required to avoid a dead lock in case two different
  // operations are running concurrently:
  //     popBackPushFront(L1 -> L2)
  //     popBackPushFront(L2 -> L1)
  // TODO: this has to be improved indeed it avoid the concurrency
  //       of every other popBackPushFront operations on different
  //       containers
  // clang-format on

  const boost::lock_guard myLock(thePopBackPushFrontMutex);

  Base::theApplyer.performOnExisting(
      aSourceName,
      [this, &aDestinationName, &myRet](Container& aSourceContainer) {
        if (aSourceContainer.empty()) {
          return;
        }
        myRet = aSourceContainer.back();
        aSourceContainer.pop_back();

        const std::string myValue = myRet.value();

        Base::theApplyer.performOnNew(
            aDestinationName, [&myValue](Container& aDestinationContainer) {
              aDestinationContainer.push_front(myValue);
            });
      });

  return myRet;
}
