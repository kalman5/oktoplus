#pragma once

#include "Commands/commands_server_service.h"
#include "Storage/sequencecontainer.h"

#include <grpc++/grpc++.h>

#include <memory>

namespace okts::cmds {

class CommandsList : virtual public Interface::Service
{
 public:
  CommandsList();

 private:
  // BLMOVE
  // BLMPOP
  // BLPOP
  // BRPOP
  // BRPOPLPUSH

  grpc::Status
  listIndex(grpc::ServerContext*, const IndexRequest*, GetValueReply*) final;

  grpc::Status
  listInsert(grpc::ServerContext*, const InsertRequest*, InsertReply*) final;

  grpc::Status
  listLength(grpc::ServerContext*, const LengthRequest*, LengthReply*) final;

  grpc::Status
  listMove(grpc::ServerContext*, const MoveRequest*, GetValueReply*) final;

  grpc::Status listMultiplePop(grpc::ServerContext*,
                               const MultiplePopRequest*,
                               MultiplePopReply*) final;

  grpc::Status listPopFront(grpc::ServerContext*,
                            const PopFrontRequest*,
                            PopFrontReply*) final;

  grpc::Status listPosition(grpc::ServerContext*,
                            const PositionRequest*,
                            PositionReply*) final;

  grpc::Status
  listPushFront(grpc::ServerContext*, const PushRequest*, PushReply*) final;

  grpc::Status
  listExistPushBack(grpc::ServerContext*, const PushRequest*, PushReply*) final;

  grpc::Status
  listRange(grpc::ServerContext*, const RangeRequest*, RangeReply*) final;

  grpc::Status
  listRemove(grpc::ServerContext*, const RemoveRequest*, RemoveReply*) final;

  grpc::Status
  listSet(grpc::ServerContext*, const SetRequest*, SetReply*) final;

  grpc::Status
  listTrim(grpc::ServerContext*, const TrimRequest*, TrimReply*) final;

  grpc::Status
  listPopBack(grpc::ServerContext*, const PopBackRequest*, PopBackReply*) final;

  grpc::Status
  listPushBack(grpc::ServerContext*, const PushRequest*, PushReply*) final;

  grpc::Status listExistPushFront(grpc::ServerContext*,
                                  const PushRequest*,
                                  PushReply*) final;

  stor::Lists theLists;
};

} // namespace okts::cmds
