#include "Configurations/commandlineconfiguration.h"
#include "Configurations/jsonconfiguration.h"
#include "Resp/resp_server.h"
#include "Storage/storage_context.h"
#include "Support/googleraii.h"

#ifdef OKTOPLUS_WITH_GRPC
#include "Commands/commands_server.h"
#endif

#include <glog/logging.h>

#include <memory>

// Bake jemalloc tuning into the binary so it isn't fragile to the
// environment (`MALLOC_CONF=...`).
//
//   narenas:1            -- collapse jemalloc's default `4 * CPU`
//                           arenas down to one. Each arena carries
//                           its own metadata trees; one arena instead
//                           of ~64 saves ~2.7 MiB of baseline RSS on
//                           our reference box. Hot-path throughput
//                           was unchanged in measurement (LPUSH /
//                           RPUSH at -c 50 -P 16 hit identical rps to
//                           the default config) because tcache
//                           absorbs almost every allocation before it
//                           touches the arena mutex.
//   muzzy_decay_ms:0     -- skip the "muzzy" intermediate state and
//                           hand pages straight from dirty to OS, so
//                           RSS reflects what we actually retain.
//
// Defined as a weak symbol jemalloc looks up at startup; harmless in
// builds linked against glibc malloc (the symbol is just unused).
extern "C" {
const char* malloc_conf
    __attribute__((weak)) = "narenas:1,muzzy_decay_ms:0";
}

namespace okcfgs = okts::cfgs;
namespace okresp = okts::resp;
namespace oksu   = okts::sup;
namespace okstor = okts::stor;
#ifdef OKTOPLUS_WITH_GRPC
namespace okcmds = okts::cmds;
#endif

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

#ifdef OKTOPLUS_WITH_GRPC
    std::unique_ptr<okcmds::CommandsServer> myGrpcServer;
    if (myOktoplusConfiguration->hasGrpcEndpoint()) {
      myGrpcServer = std::make_unique<okcmds::CommandsServer>(
          myStorage,
          myOktoplusConfiguration->endpoint(),
          myOktoplusConfiguration->numCqs(),
          myOktoplusConfiguration->minPollers(),
          myOktoplusConfiguration->maxPollers());
    } else if (!myRespServer) {
      throw std::runtime_error(
          "no wire protocol configured -- set `service.resp_endpoint` "
          "and/or `service.endpoint` in the config");
    }
#else
    if (myOktoplusConfiguration->hasGrpcEndpoint()) {
      LOG(WARNING) << "service.endpoint is set but this oktoplus build "
                      "was compiled with OKTOPLUS_WITH_GRPC=OFF; "
                      "ignoring and serving RESP only.";
    }
    if (!myRespServer) {
      throw std::runtime_error(
          "no wire protocol configured -- set `service.resp_endpoint` "
          "in the config (this build has no gRPC server)");
    }
#endif

    // Block on whichever server is configured. Both `wait()`s have the
    // same contract: they return only when the server is shut down
    // (currently only via process termination -- ~Server is invoked by
    // stack unwind on signal-driven exit).
#ifdef OKTOPLUS_WITH_GRPC
    if (myGrpcServer) {
      myGrpcServer->wait();
    } else {
      myRespServer->wait();
    }
#else
    myRespServer->wait();
#endif

    return EXIT_SUCCESS;

  } catch (const std::exception& e) {
    LOG(ERROR) << "Error: " << e.what();
  } catch (...) {
    LOG(ERROR) << "Error: unknown";
  }

  return EXIT_FAILURE;
}
