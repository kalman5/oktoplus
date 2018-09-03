#pragma once

#include "Configurations/mainconfiguration.h"
#include "Support/noncopyable.h"

#include <string>

namespace okts {
namespace cfg {

class JsonConfiguration : public MainConfiguration
{
 public:
  DISABLE_EVIL_CONSTRUCTOR(JsonConfiguration);

  JsonConfiguration(const std::string& aPathConfigurationFile);
  ~JsonConfiguration() override = default;

  void dump() override;
};

} // namespace cfg
} // namespace okts
