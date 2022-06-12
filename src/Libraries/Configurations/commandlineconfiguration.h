#pragma once

#include "Support/noncopyable.h"

#include <string>

namespace okts::cfgs {

class CommandLineConfiguration
{
 public:
  DISABLE_EVIL_CONSTRUCTOR(CommandLineConfiguration);

  CommandLineConfiguration(int aArgc, char** aArgv);

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

} // namespace okts::cfgs
