#pragma once

#include "Commands/commands_list.h"

#include <grpc++/grpc++.h>

#include <memory>

namespace oktoplus {
namespace commands {

class CommandsServer : public CommandsList
{
 public:
  CommandsServer(const std::string& myEndpoint);

  void wait();

  void shutdown();

 private:
  std::shared_ptr<::grpc::ServerCredentials> theCredentials;
  std::unique_ptr<::grpc::Server>            theServer;
};

} // namespace commands
} // namespace octoplus
