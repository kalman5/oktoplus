#pragma once

#include "Commands/commands_server_service.h"
#include "Storage/sets.h"

#include <grpc++/grpc++.h>

#include <memory>

namespace okts::cmds {

class CommandsSet : virtual public Interface::Service
{
 public:
  CommandsSet();

 private:
  grpc::Status setAdd(grpc::ServerContext*,
                      const SetAddRequest*,
                      SetAddReply*) final override;

  grpc::Status setCard(grpc::ServerContext*,
                       const SetCardRequest*,
                       SetCardReply*) final override;

  grpc::Status setDiff(grpc::ServerContext*,
                       const SetDiffRequest*,
                       SetDiffReply*) final override;

  // SDIFFSTORE
  // SINTER
  // SINTERSTORE
  // SISMEMBER
  // SMEMBERS
  // SMOVE
  // SPOP
  // SRANDMEMBER
  // SREM
  // SSCAN
  // SUNION
  // SUNIONSTORE

  stor::Sets theSets;
};

} // namespace okts::cmds
