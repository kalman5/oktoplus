#pragma once

#include "Commands/commands_deque.h"
#include "Commands/commands_list.h"
#include "Commands/commands_vector.h"

#include <grpc++/grpc++.h>

#include <memory>

namespace okts {
namespace cmds {

class CommandsServer : public CommandsDeque,
                       public CommandsList,
                       public CommandsVector
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
