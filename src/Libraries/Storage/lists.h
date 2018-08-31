#pragma once

#include "Storage/backoperations.h"
#include "Storage/frontonlyoperations.h"

#include "Support/non_copyable.h"

#include <list>
#include <string>

namespace okts {
namespace storage {

class Lists : public BackOperations<std::list<std::string>>,
              public FrontOnlyOperations<std::list<std::string>>
{
  using Base = BackOperations<std::list<std::string>>;
  using Base2 = FrontOnlyOperations<std::list<std::string>>;
 public:
  DISABLE_EVIL_CONSTRUCTOR(Lists);

  Lists();
};

} // namespace storage
} // namespace okts
