#pragma once

#include "Support/containerfunctorapplier.h"
#include "Support/non_copyable.h"

#include <boost/optional.hpp>
#include <boost/thread/mutex.hpp>

#include <functional>
#include <list>
#include <string_view>

namespace oktoplus {
namespace storage {

class Vectors
{
 public:
  DISABLE_EVIL_CONSTRUCTOR(Vectors);

  enum class Position { BEFORE = 0, AFTER = 1 };
  enum class Status { OK = 0, NOT_FOUND = 1, OUT_OF_RANGE = 2 };

  Vectors();

  size_t hostedKeys() const;

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

 private:
  using Vector = std::list<std::string>;

  support::ContainerFunctorApplier<Vector> theApplyer;
};

} // namespace storage
} // namespace oktoplus
