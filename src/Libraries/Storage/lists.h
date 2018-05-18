#pragma once

#include "Support/non_copyable.h"

#include <boost/thread.hpp>

#include <functional>
#include <list>
#include <string_view>
#include <unordered_map>

namespace oktoplus {
namespace storage {

class Lists
{
 public:
  DISABLE_EVIL_CONSTRUCTOR(Lists);

  enum class Position { BEFORE = 0, AFTER = 1 };

  Lists();

  size_t pushBack(const std::string&                   aName,
                  const std::vector<std::string_view>& aValues);

  size_t pushFront(const std::string&                   aName,
                   const std::vector<std::string_view>& aValues);

  boost::optional<std::string> popBack(const std::string& aName);

  boost::optional<std::string> popFront(const std::string& aName);

  boost::optional<size_t> size(const std::string& aName);

  boost::optional<std::string> index(const std::string& aName, int64_t aIndex);

  boost::optional<int64_t> insert(const std::string& aName,
                                  Position           aPosition,
                                  const std::string& aPivot,
                                  const std::string& aValue);

 private:
  using ProtectedList = std::pair<boost::mutex, std::list<std::string>>;
  using Storage       = std::unordered_map<std::string, ProtectedList>;

  using Functor = std::function<void(ProtectedList& aList)>;

  void performOnNew(const std::string& aName, const Functor& aFunctor);
  void performOnExisting(const std::string& aName, const Functor& aFunctor);

  boost::mutex theMutex;
  Storage      theStorage;
};

} // namespace storage
} // namespace oktoplus
