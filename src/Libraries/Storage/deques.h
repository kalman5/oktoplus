#pragma once

#include <deque>
#include <string>

#include "Storage/backoperations.h"
#include "Storage/frontonlyoperations.h"
#include "Support/noncopyable.h"

namespace okts {
namespace stor {

class Deques : public BackOperations<std::deque<std::string>>,
               public FrontOnlyOperations<std::deque<std::string>>
{
  using Base  = BackOperations<std::deque<std::string>>;
  using Base2 = FrontOnlyOperations<std::deque<std::string>>;

 public:
  DISABLE_EVIL_CONSTRUCTOR(Deques);

  Deques();
};

} // namespace stor
} // namespace okts
