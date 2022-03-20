#pragma once

#include "Support/containerfunctorapplier.h"
#include "Support/noncopyable.h"

namespace okts::stor {

template <class CONTAINER>
class GenericContainer
{
 public:
  DISABLE_EVIL_CONSTRUCTOR(GenericContainer);

  enum class Position { BEFORE = 0, AFTER = 1 };
  enum class Status { OK = 0, NOT_FOUND = 1, OUT_OF_RANGE = 2 };

  GenericContainer()          = default;
  virtual ~GenericContainer() = default;

  size_t hostedKeys() const {
    return theApplyer.hostedKeys();
  }

 protected:
  sup::ContainerFunctorApplier<CONTAINER> theApplyer;
};

} // namespace okts::stor
