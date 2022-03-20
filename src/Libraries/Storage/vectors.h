#pragma once

#include <string>
#include <vector>

#include "Storage/backoperations.h"
#include "Support/noncopyable.h"

namespace okts::stor {

class Vectors : public BackOperations<std::vector<std::string>>
{
  using Base = BackOperations<std::vector<std::string>>;

 public:
  DISABLE_EVIL_CONSTRUCTOR(Vectors);

  Vectors();
};

} // namespace okts::stor
