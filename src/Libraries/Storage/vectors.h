#pragma once

#include "Storage/backoperations.h"

#include "Support/noncopyable.h"

#include <string>
#include <vector>

namespace okts {
namespace stor {

class Vectors : public BackOperations<std::vector<std::string>>
{
  using Base = BackOperations<std::vector<std::string>>;

 public:
  DISABLE_EVIL_CONSTRUCTOR(Vectors);

  Vectors();
};

} // namespace stor
} // namespace okts
