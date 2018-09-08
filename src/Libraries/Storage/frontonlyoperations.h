#pragma once

#include "Support/noncopyable.h"

#include <boost/optional.hpp>
#include <boost/thread/mutex.hpp>

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

  size_t pushFront(const std::string&                   aName,
                   const std::vector<std::string_view>& aValues);

  boost::optional<std::string> popFront(const std::string& aName);

  size_t pushFrontExist(const std::string&                   aName,
                        const std::vector<std::string_view>& aValues);

  boost::optional<std::string>
  popBackPushFront(const std::string& aSourceName,
                   const std::string& aDestinationName);

 private:
  using PopBackPushFrontMutex = boost::mutex;
  PopBackPushFrontMutex thePopBackPushFrontMutex;
};

#include "Storage/frontonlyoperations.inl"

} // namespace stor
} // namespace okts
