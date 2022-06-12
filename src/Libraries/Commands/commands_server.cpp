#include "Commands/commands_server.h"

#include <glog/logging.h>

#include "Support/logo.h"

#include <sstream>
#include <string_view>

namespace okts {
namespace cmds {

CommandsServer::CommandsServer(const std::string& aEndpoint,
                               int                aNumCQS,
                               int                aMinPollers,
                               int                aMaxPollers)
    : theCredentials(::grpc::InsecureServerCredentials())
    , theServer() {
  ::grpc::ServerBuilder myBuilder;
  myBuilder.SetSyncServerOption(::grpc::ServerBuilder::NUM_CQS, aNumCQS)
      .SetSyncServerOption(::grpc::ServerBuilder::MIN_POLLERS, aMinPollers)
      .SetSyncServerOption(::grpc::ServerBuilder::MAX_POLLERS, aMaxPollers)
      .AddListeningPort(aEndpoint, theCredentials)
      .RegisterService(this);
  theServer = myBuilder.BuildAndStart();

  sup::logo(aEndpoint, aNumCQS, aMinPollers, aMaxPollers);

  const auto myLogService = "Oktoplus service on " + aEndpoint;

  if (theServer) {
    LOG(INFO) << "Started.";
  } else {
    std::stringstream myError;
    myError << "Could not start " << myLogService;
    LOG(ERROR) << myError.str();
    throw std::runtime_error(myError.str());
  }
}

void CommandsServer::wait() {
  theServer->Wait();
}

void CommandsServer::shutdown() {
  theServer->Shutdown();
}

} // namespace cmds
} // namespace okts
