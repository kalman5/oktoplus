#include "Configurations/commandlineconfiguration.h"

#include "Configurations/defaultconfiguration.h"

#include <boost/program_options.hpp>

#include <fstream>
#include <iostream>

namespace bpo = boost::program_options;

namespace okts {
namespace cfgs {

CommandLineConfiguration::CommandLineConfiguration(int aArgc, char** aArgv)
    : theGenerateDefaultConfigurationFile(false)
    , theHelpRequested(false)
    , theConfigurationFilePath("oktoplus.cfg")
    , theConfigurationFileSpecified(false) {

  DefaultConfiguration myDefaultConfiguration;

  bpo::options_description desc("Allowed options");

  // clang-format off
  desc.add_options()
      ("help", "produce help message")
      ("template,m", "generate a default json configuration file")
      ("conf,c",
       bpo::value<std::string>(&theConfigurationFilePath),
       "configuration file path")
  ;
  // clang-format on

  bpo::variables_map vm;
  bpo::store(bpo::parse_command_line(aArgc, aArgv, desc), vm);
  bpo::notify(vm);

  if (vm.count("help")) {
    theHelpRequested = true;
    std::cout << desc << std::endl;
    return;
  }

  if (vm.count("template")) {
    theGenerateDefaultConfigurationFile = true;
  }

  if (vm.count("conf")) {
    theConfigurationFileSpecified = true;
  }
}

} // namespace cfgs
} // namespace okts
