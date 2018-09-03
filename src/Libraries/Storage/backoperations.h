#pragma once

#include "Storage/genericcontainer.h"

#include "Support/noncopyable.h"

#include <boost/optional.hpp>

#include <string>
#include <string_view>

namespace okts {
namespace storage {

template <class CONTAINER>
class BackOperations : virtual public GenericContainer<CONTAINER>
{
  using Container = CONTAINER;
  using Base      = GenericContainer<Container>;

 public:
  using Status   = typename Base::Status;
  using Position = typename Base::Position;

  DISABLE_EVIL_CONSTRUCTOR(BackOperations);

  BackOperations()
      : Base() {
  }

  size_t pushBack(const std::string&                   aName,
                  const std::vector<std::string_view>& aValues);

  boost::optional<std::string> popBack(const std::string& aName);

  size_t size(const std::string& aName) const;

  boost::optional<std::string> index(const std::string& aName,
                                     int64_t            aIndex) const;

  boost::optional<int64_t> insert(const std::string& aName,
                                  Position           aPosition,
                                  const std::string& aPivot,
                                  const std::string& aValue);

  std::vector<std::string>
  range(const std::string& aName, int64_t aStart, int64_t aStop) const;

  size_t
  remove(const std::string& aName, int64_t aCount, const std::string& aValue);

  Status
  set(const std::string& aName, int64_t aIndex, const std::string& aValue);

  void trim(const std::string& aName, int64_t aStart, int64_t aStop);

  size_t pushBackExist(const std::string&                   aName,
                       const std::vector<std::string_view>& aValues);
};

#include "Storage/backoperations.inl"

} // namespace storage
} // namespace okts
