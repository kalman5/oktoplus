#pragma once

#include <list>
#include <string>

#include "Storage/backoperations.h"
#include "Storage/frontonlyoperations.h"
#include "Support/noncopyable.h"

namespace okts::stor {

class Lists : public BackOperations<std::list<std::string>>,
              public FrontOnlyOperations<std::list<std::string>>
{
  using Base  = BackOperations<std::list<std::string>>;
  using Base2 = FrontOnlyOperations<std::list<std::string>>;

 public:
  DISABLE_EVIL_CONSTRUCTOR(Lists);

  Lists();
};

} // namespace okts::stor
