#pragma once

#include "Configurations/oktoplusconfiguration.h"
#include "Support/noncopyable.h"

#include <string>

namespace okts {
namespace cfgs {

class CommandLineConfiguration : public OktoplusConfiguration
{
 public:
  DISABLE_EVIL_CONSTRUCTOR(CommandLineConfiguration);

  CommandLineConfiguration(int aArgc, char** aArgv);
  ~CommandLineConfiguration() override = default;

  bool generateFile() const {
    return theGenerateDefaultConfigurationFile;
  }

  bool helpRequested() const {
    return theHelpRequested;
  }

  const std::string configurationFile() const {
    return theConfigurationFilePath;
  }

  bool configurationFileSpecified() const {
    return theConfigurationFileSpecified;
  }

 private:
  bool theGenerateDefaultConfigurationFile;
  bool theHelpRequested;

  std::string theConfigurationFilePath;
  bool        theConfigurationFileSpecified;
};

} // namespace cfgs
} // namespace okts
