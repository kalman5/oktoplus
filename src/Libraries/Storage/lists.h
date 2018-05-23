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

  size_t hostedKeys() const;

  size_t pushBack(const std::string&                   aName,
                  const std::vector<std::string_view>& aValues);

  size_t pushFront(const std::string&                   aName,
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

 private:
  using List = std::list<std::string>;
  struct ProtectedList {
    ProtectedList()
        : mutex(new boost::mutex())
        , list() {
    }
    std::unique_ptr<boost::mutex> mutex;
    List                          list;
  };

  using Storage      = std::unordered_map<std::string, ProtectedList>;
  using Functor      = std::function<void(List& aList)>;
  using ConstFunctor = std::function<void(const List& aList)>;

  void performOnNew(const std::string& aName, const Functor& aFunctor);
  void performOnExisting(const std::string& aName, const Functor& aFunctor);
  void performOnExisting(const std::string&  aName,
                         const ConstFunctor& aFunctor) const;

  mutable boost::mutex theMutex;
  Storage              theStorage;
};

} // namespace storage
} // namespace oktoplus
