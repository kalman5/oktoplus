#pragma once

#include "Storage/backoperations.h"
#include "Storage/frontonlyoperations.h"

#include "Support/noncopyable.h"

#include <deque>
#include <string>

namespace okts {
namespace storage {

class Deques : public BackOperations<std::deque<std::string>>,
               public FrontOnlyOperations<std::deque<std::string>>
{
  using Base  = BackOperations<std::deque<std::string>>;
  using Base2 = FrontOnlyOperations<std::deque<std::string>>;

 public:
  DISABLE_EVIL_CONSTRUCTOR(Deques);

  Deques();
};

} // namespace storage
} // namespace okts
