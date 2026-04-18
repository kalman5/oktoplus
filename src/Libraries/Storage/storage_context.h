#pragma once

#include "Storage/sequencecontainer.h"
#include "Storage/sets.h"

namespace okts::stor {

struct StorageContext {
  Lists   lists;
  Deques  deques;
  Vectors vectors;
  Sets    sets;
};

} // namespace okts::stor
