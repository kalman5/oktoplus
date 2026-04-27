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

    // RESP is the primary wire protocol and always runs. gRPC is now
    // opt-in: omit `service.endpoint` from the config to disable it.
    std::unique_ptr<okresp::RespServer> myRespServer;
    if (myOktoplusConfiguration->hasRespEndpoint()) {
      myRespServer = std::make_unique<okresp::RespServer>(
          myStorage, myOktoplusConfiguration->respEndpoint());
    }

    std::unique_ptr<okcmds::CommandsServer> myGrpcServer;
    if (myOktoplusConfiguration->hasGrpcEndpoint()) {
      myGrpcServer = std::make_unique<okcmds::CommandsServer>(
          myStorage,
          myOktoplusConfiguration->endpoint(),
          myOktoplusConfiguration->numCqs(),
          myOktoplusConfiguration->minPollers(),
          myOktoplusConfiguration->maxPollers());
    }

    if (!myGrpcServer && !myRespServer) {
      throw std::runtime_error(
          "no wire protocol configured -- set `service.resp_endpoint` "
          "and/or `service.endpoint` in the config");
    }

    // Block on whichever server is configured. Both `wait()`s have the
    // same contract: they return only when the server is shut down
    // (currently only via process termination -- ~Server is invoked by
    // stack unwind on signal-driven exit).
    if (myGrpcServer) {
      myGrpcServer->wait();
    } else {
      myRespServer->wait();
    }

    return EXIT_SUCCESS;

  } catch (const std::exception& e) {
    LOG(ERROR) << "Error: " << e.what();
  } catch (...) {
    LOG(ERROR) << "Error: unknown";
  }

  return EXIT_FAILURE;
}
