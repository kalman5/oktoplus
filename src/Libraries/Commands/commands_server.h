#pragma once

#include "Commands/commands_deque.h"
#include "Commands/commands_list.h"
#include "Commands/commands_set.h"
#include "Commands/commands_vector.h"
#include "Storage/storage_context.h"

#include <grpc++/grpc++.h>

#include <memory>

namespace okts {
namespace cmds {

class CommandsServer : public CommandsDeque,
                       public CommandsList,
                       public CommandsVector,
                       public CommandsSet
{
 public:
  CommandsServer(stor::StorageContext& aStorage,
                 const std::string&    aEndpoint,
                 int                   aNumCQS,
                 int                   aMinPollers,
                 int                   aMaxPollers);

  void wait();

  void shutdown();

 private:
  std::shared_ptr<::grpc::ServerCredentials> theCredentials;
  std::unique_ptr<::grpc::Server>            theServer;
};

} // namespace cmds
} // namespace okts
