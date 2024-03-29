#include "Commands/commands_server.h"
#include "Configurations/commandlineconfiguration.h"
#include "Configurations/jsonconfiguration.h"
#include "Support/googleraii.h"

#include <glog/logging.h>

namespace okcmds = okts::cmds;
namespace okcfgs = okts::cfgs;
namespace oksu   = okts::sup;

int main(int argc, char** argv) {

  oksu::GoogleRaii myShutdowner(argv[0], true, true);

  try {
    okcfgs::CommandLineConfiguration myCommandLine(argc, argv);

    if (myCommandLine.helpRequested()) {
      return EXIT_SUCCESS;
    }

    if (myCommandLine.generateFile()) {
      okcfgs::OktoplusConfiguration myDefault;
      okcfgs::JsonConfiguration     myJson(myDefault);
      myJson.dump(myCommandLine.configurationFile());
      return EXIT_SUCCESS;
    }

    std::unique_ptr<okcfgs::OktoplusConfiguration> myOktoplusConfiguration;

    if (myCommandLine.configurationFileSpecified()) {
      myOktoplusConfiguration = std::make_unique<okcfgs::JsonConfiguration>(
          myCommandLine.configurationFile());
    } else {
      throw std::runtime_error("Configuration file was not specified");
    }

    okcmds::CommandsServer myServer(myOktoplusConfiguration->endpoint(),
                                    myOktoplusConfiguration->numCqs(),
                                    myOktoplusConfiguration->minPollers(),
                                    myOktoplusConfiguration->maxPollers());

    myServer.wait();

    return EXIT_SUCCESS;

  } catch (const std::exception& e) {
    LOG(ERROR) << "Error: " << e.what();
  } catch (...) {
    LOG(ERROR) << "Error: unknown";
  }

  return EXIT_FAILURE;
}
