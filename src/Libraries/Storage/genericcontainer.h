#pragma once

#include "Support/containerfunctorapplier.h"
#include "Support/non_copyable.h"

#include <boost/optional.hpp>
#include <boost/thread/mutex.hpp>

#include <functional>
#include <list>
#include <string_view>

namespace oktoplus {
namespace storage {

template <class CONTAINER>
class GenericContainer
{
 public:
  DISABLE_EVIL_CONSTRUCTOR(GenericContainer);

  enum class Position { BEFORE = 0, AFTER = 1 };
  enum class Status { OK = 0, NOT_FOUND = 1, OUT_OF_RANGE = 2 };

  GenericContainer() = default;
  virtual ~GenericContainer() = default;

  size_t hostedKeys() const {
    return theApplyer.hostedKeys();
  }

 protected:
  support::ContainerFunctorApplier<CONTAINER> theApplyer;
};

} // namespace storage
} // namespace oktoplus
