#pragma once

#include "Support/noncopyable.h"

#include <boost/thread/mutex.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace okts {
namespace stor {

template <class CONTAINER>
class FrontOnlyOperations : virtual public GenericContainer<CONTAINER>
{
  using Container = CONTAINER;
  using Base      = GenericContainer<Container>;

 public:
  DISABLE_EVIL_CONSTRUCTOR(FrontOnlyOperations);

  FrontOnlyOperations()
      : Base() {
  }

  size_t pushFront(const std::string_view&              aName,
                   const std::vector<std::string_view>& aValues);

  std::optional<std::string> popFront(const std::string_view& aName);

  size_t pushFrontExist(const std::string_view&              aName,
                        const std::vector<std::string_view>& aValues);

  std::optional<std::string>
  popBackPushFront(const std::string_view& aSourceName,
                   const std::string_view& aDestinationName);

 private:
  using PopBackPushFrontMutex = boost::mutex;
  PopBackPushFrontMutex thePopBackPushFrontMutex;
};

#include "Storage/frontonlyoperations.inl"

} // namespace stor
} // namespace okts
