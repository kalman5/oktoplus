#pragma once

#include "Storage/backoperations.h"

#include "Support/non_copyable.h"

#include <string>
#include <vector>

namespace oktoplus {
namespace storage {

class Vectors : public BackOperations<std::vector<std::string>>
{
  using Base = BackOperations<std::vector<std::string>>;
 public:
  DISABLE_EVIL_CONSTRUCTOR(Vectors);

  Vectors();
};

} // namespace storage
} // namespace oktoplus
