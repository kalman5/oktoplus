#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "Storage/genericcontainer.h"
#include "Support/noncopyable.h"

namespace okts::stor {

class Sets : public GenericContainer<std::unordered_set<std::string>>
{
  using Container = std::unordered_set<std::string>;
  using Base      = GenericContainer<Container>;

 public:
  DISABLE_EVIL_CONSTRUCTOR(Sets);

  Sets();

  size_t add(const std::string&                   aName,
             const std::vector<std::string_view>& aValues);

  size_t cardinality(const std::string& aName) const;

  Container diff(const std::vector<std::string_view>& aNames);
};

} // namespace okts::stor
