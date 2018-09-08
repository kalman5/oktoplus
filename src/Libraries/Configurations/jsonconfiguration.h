#pragma once

#include "Configurations/oktoplusconfiguration.h"

#include "Support/noncopyable.h"

#include <string>

namespace okts {
namespace cfgs {

// class OktoplusConfiguration;

class JsonConfiguration : public OktoplusConfiguration
{
 public:
  DISABLE_EVIL_CONSTRUCTOR(JsonConfiguration);

  JsonConfiguration(const std::string& aConfigurationFile);
  JsonConfiguration(const OktoplusConfiguration& aConfiguration);

  ~JsonConfiguration() override = default;

  void dump();
  void dump(const std::string& aPath);

 private:
  const std::string theConfigurationFile;
};

} // namespace cfgs
} // namespace okts
