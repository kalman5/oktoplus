#pragma once

#include "Storage/genericcontainer.h"

#include "Support/noncopyable.h"

#include <unordered_set>
#include <string>

namespace okts {
namespace stor {

class Sets : public GenericContainer<std::unordered_set<std::string>>
{
  using Base  = GenericContainer<std::unordered_set<std::string>>;

 public:
  DISABLE_EVIL_CONSTRUCTOR(Sets);

  Sets();
};

} // namespace stor
} // namespace okts
