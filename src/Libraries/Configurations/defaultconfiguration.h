#pragma once

#include "Configurations/oktoplusconfiguration.h"
#include "Support/noncopyable.h"

#include <string>

namespace okts {
namespace cfgs {

class DefaultConfiguration : public OktoplusConfiguration
{
 public:
  DISABLE_EVIL_CONSTRUCTOR(DefaultConfiguration);

  DefaultConfiguration();
  ~DefaultConfiguration() override = default;
};

} // namespace cfgs
} // namespace okts
