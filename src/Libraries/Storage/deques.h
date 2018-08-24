#pragma once

#include "Storage/backoperations.h"
#include "Storage/frontonlyoperations.h"

#include "Support/non_copyable.h"

#include <deque>
#include <string>

namespace oktoplus {
namespace storage {

class Deques : public BackOperations<std::deque<std::string>>,
               public FrontOnlyOperations<std::deque<std::string>>
{
  using Base = BackOperations<std::deque<std::string>>;
  using Base2 = FrontOnlyOperations<std::deque<std::string>>;
 public:
  DISABLE_EVIL_CONSTRUCTOR(Deques);

  Deques();
};

} // namespace storage
} // namespace oktoplus
