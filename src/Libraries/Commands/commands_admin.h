#pragma once

#include "Commands/commands_server_service.h"
#include "Storage/storage_context.h"

#include <grpc++/grpc++.h>

namespace okts::cmds {

class CommandsAdmin : virtual public Interface::Service
{
 public:
  explicit CommandsAdmin(stor::StorageContext& aStorage);

 private:
  grpc::Status flushAll(grpc::ServerContext*,
                        const Void*,
                        Void*) final override;

  grpc::Status flushDb(grpc::ServerContext*,
                       const Void*,
                       Void*) final override;

  stor::StorageContext& theStorage;
};

} // namespace okts::cmds
