#pragma once

#include "Commands/commands_list.h"

#include <grpc++/grpc++.h>

#include <memory>

namespace okts {
namespace cmds {

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

} // namespace cmds
} // namespace okts
