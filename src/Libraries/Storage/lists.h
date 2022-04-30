#pragma once

#include <list>
#include <string>

#include "Storage/backoperations.h"
#include "Support/noncopyable.h"

namespace okts::stor {

using Lists = BackOperations<std::list<std::string>>;

} // namespace okts::stor
