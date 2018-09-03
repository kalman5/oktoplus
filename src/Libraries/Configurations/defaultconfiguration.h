#pragma once

#include "Configurations/mainconfiguration.h"
#include "Support/noncopyable.h"

#include <string>

namespace okts {
namespace cfg {

class DefaultConfiguration : public MainConfiguration
{
 public:
  DISABLE_EVIL_CONSTRUCTOR(DefaultConfiguration);

  DefaultConfiguration();
  ~DefaultConfiguration() override = default;

  void dump() override;
};

} // namespace cfg
} // namespace okts
