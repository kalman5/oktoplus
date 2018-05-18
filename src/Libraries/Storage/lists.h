#pragma once

#include "Support/non_copyable.h"

#include <boost/thread.hpp>

#include <functional>
#include <list>
#include <unordered_map>
#include <string_view>

namespace oktoplus {
namespace storage {

class Lists
{
 public:
  DISABLE_EVIL_CONSTRUCTOR(Lists);

  Lists();

  size_t pushBack(const std::string&                   aName,
                  const std::vector<std::string_view>& aValues);

  size_t pushFront(const std::string&                   aName,
                   const std::vector<std::string_view>& aValues);

  boost::optional<std::string> popBack(const std::string& aName);

  boost::optional<std::string> popFront(const std::string& aName);

  boost::optional<size_t> size(const std::string& aName);

  boost::optional<std::string> index(const std::string& aName, int64_t aIndex);

 private:
  using ProtectedList = std::pair<boost::mutex, std::list<std::string>>;
  using Storage = std::unordered_map<std::string, ProtectedList>;

  using Functor = std::function<void(ProtectedList& aList)>;

  void performOnNew(const std::string& aName, const Functor& aFunctor);
  void performOnExisting(const std::string& aName, const Functor& aFunctor);

  boost::mutex theMutex;
  Storage      theStorage;
};

} // namespace storage
} // namespace octoplus
