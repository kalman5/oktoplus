#include "Commands/commands_server.h"

#include <glog/logging.h>

#include <sstream>
#include <string_view>

namespace okts {
namespace cmds {

CommandsServer::CommandsServer(const std::string& myEndpoint)
    : theCredentials(::grpc::InsecureServerCredentials())
    , theServer() {
  ::grpc::ServerBuilder myBuilder;
  myBuilder.SetSyncServerOption(::grpc::ServerBuilder::NUM_CQS, 20)
      .SetSyncServerOption(::grpc::ServerBuilder::MIN_POLLERS, 10)
      .SetSyncServerOption(::grpc::ServerBuilder::MAX_POLLERS, 30)
      .AddListeningPort(myEndpoint, theCredentials)
      .RegisterService(this);
  theServer = myBuilder.BuildAndStart();

  const auto myLogService = "Oktoplus service on " + myEndpoint;

  if (theServer) {
    LOG(INFO) << myLogService << ". Started.";
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
