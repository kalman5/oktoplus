#pragma once

#include "Commands/commands_server_service.h"
#include "Storage/sequencecontainer.h"

#include <grpc++/grpc++.h>

#include <memory>

namespace okts::cmds {

class CommandsDeque : virtual public Interface::Service
{
 public:
  CommandsDeque();

 private:
  // BLMOVE
  // BLMPOP
  // BLPOP
  // BRPOP
  // BRPOPLPUSH

  grpc::Status
  dequeIndex(grpc::ServerContext*, const IndexRequest*, GetValueReply*) final;

  grpc::Status
  dequeInsert(grpc::ServerContext*, const InsertRequest*, InsertReply*) final;

  grpc::Status
  dequeLength(grpc::ServerContext*, const LengthRequest*, LengthReply*) final;

  grpc::Status
  dequeMove(grpc::ServerContext*, const MoveRequest*, GetValueReply*);

  // LMPOP

  grpc::Status dequePopFront(grpc::ServerContext*,
                             const PopFrontRequest*,
                             PopFrontReply*) final;

  grpc::Status dequePosition(grpc::ServerContext*,
                             const PositionRequest*,
                             PositionReply*) final;

  grpc::Status
  dequePushFront(grpc::ServerContext*, const PushRequest*, PushReply*) final;

  grpc::Status dequeExistPushBack(grpc::ServerContext*,
                                  const PushRequest*,
                                  PushReply*) final;

  grpc::Status
  dequeRange(grpc::ServerContext*, const RangeRequest*, RangeReply*) final;

  grpc::Status
  dequeRemove(grpc::ServerContext*, const RemoveRequest*, RemoveReply*) final;

  grpc::Status
  dequeSet(grpc::ServerContext*, const SetRequest*, SetReply*) final;

  grpc::Status
  dequeTrim(grpc::ServerContext*, const TrimRequest*, TrimReply*) final;

  grpc::Status dequePopBack(grpc::ServerContext*,
                            const PopBackRequest*,
                            PopBackReply*) final;

  grpc::Status
  dequePushBack(grpc::ServerContext*, const PushRequest*, PushReply*) final;

  grpc::Status dequeExistPushFront(grpc::ServerContext*,
                                   const PushRequest*,
                                   PushReply*) final;

  stor::Deques theQueues;
};

} // namespace okts::cmds
