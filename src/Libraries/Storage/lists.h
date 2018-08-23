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

class Lists
{
 public:
  DISABLE_EVIL_CONSTRUCTOR(Lists);

  enum class Position { BEFORE = 0, AFTER = 1 };
  enum class Status { OK = 0, NOT_FOUND = 1, OUT_OF_RANGE = 2 };

  Lists();

  size_t hostedKeys() const;

  size_t pushFront(const std::string&                   aName,
                   const std::vector<std::string_view>& aValues);

  size_t pushBack(const std::string&                   aName,
                  const std::vector<std::string_view>& aValues);

  boost::optional<std::string> popBack(const std::string& aName);

  boost::optional<std::string> popFront(const std::string& aName);

  size_t size(const std::string& aName) const;

  boost::optional<std::string> index(const std::string& aName,
                                     int64_t            aIndex) const;

  boost::optional<int64_t> insert(const std::string& aName,
                                  Position           aPosition,
                                  const std::string& aPivot,
                                  const std::string& aValue);

  size_t pushFrontExist(const std::string&                   aName,
                        const std::vector<std::string_view>& aValues);

  std::vector<std::string>
  range(const std::string& aName, int64_t aStart, int64_t aStop) const;

  size_t
  remove(const std::string& aName, int64_t aCount, const std::string& aValue);

  Status
  set(const std::string& aName, int64_t aIndex, const std::string& aValue);

  void trim(const std::string& aName, int64_t aStart, int64_t aStop);

  boost::optional<std::string>
  popBackPushFront(const std::string& aSourceName,
                   const std::string& aDestinationName);

  size_t pushBackExist(const std::string&                   aName,
                       const std::vector<std::string_view>& aValues);

 private:
  using List = std::list<std::string>;

  support::ContainerFunctorApplier<List> theApplyer;

  using PopBackPushFrontMutex = boost::mutex;
  PopBackPushFrontMutex thePopBackPushFrontMutex;
};

} // namespace storage
} // namespace oktoplus
