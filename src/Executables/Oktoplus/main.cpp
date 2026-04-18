#include "Commands/commands_server.h"
#include "Configurations/commandlineconfiguration.h"
#include "Configurations/jsonconfiguration.h"
#include "Resp/resp_server.h"
#include "Storage/storage_context.h"
#include "Support/googleraii.h"

#include <glog/logging.h>

#include <memory>

namespace okcmds = okts::cmds;
namespace okcfgs = okts::cfgs;
namespace okresp = okts::resp;
namespace oksu   = okts::sup;
namespace okstor = okts::stor;

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

    okstor::StorageContext myStorage;

    okcmds::CommandsServer myServer(myStorage,
                                    myOktoplusConfiguration->endpoint(),
                                    myOktoplusConfiguration->numCqs(),
                                    myOktoplusConfiguration->minPollers(),
                                    myOktoplusConfiguration->maxPollers());

    std::unique_ptr<okresp::RespServer> myRespServer;

    if (myOktoplusConfiguration->hasRespEndpoint()) {
      myRespServer = std::make_unique<okresp::RespServer>(
          myStorage, myOktoplusConfiguration->respEndpoint());
    }

    myServer.wait();

    return EXIT_SUCCESS;

  } catch (const std::exception& e) {
    LOG(ERROR) << "Error: " << e.what();
  } catch (...) {
    LOG(ERROR) << "Error: unknown";
  }

  return EXIT_FAILURE;
}
