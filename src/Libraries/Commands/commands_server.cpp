#include "Commands/commands_server.h"

#include <glog/logging.h>

#include <sstream>
#include <string_view>

namespace okts {
namespace commands {

CommandsServer::CommandsServer(const std::string& myEndpoint)
    : theCredentials(::grpc::InsecureServerCredentials())
    , theServer() {
  ::grpc::ServerBuilder myBuilder;
  myBuilder.SetSyncServerOption(::grpc::ServerBuilder::NUM_CQS, 5)
      .SetSyncServerOption(::grpc::ServerBuilder::MIN_POLLERS, 5);
  myBuilder.AddListeningPort(myEndpoint, theCredentials);
  myBuilder.RegisterService(this);
  theServer = myBuilder.BuildAndStart();

  const auto myLogService = "Oktoplus service on " + myEndpoint;

  if (theServer) {
    LOG(INFO) << myLogService << " is ready to startup";
  } else {
    std::stringstream myError;
    myError << "Could not start " << myLogService;
    LOG(ERROR) << myError.str();
    throw std::runtime_error(myError.str());
  }
}

void CommandsServer::wait() {
  LOG(INFO) << "Server starting";
  theServer->Wait();
}

void CommandsServer::shutdown() {
  theServer->Shutdown();
}

} // namespace commands
} // namespace okts
