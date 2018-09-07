#pragma once

#include "Commands/commands_server_service.h"
#include "Storage/deques.h"

#include <grpc++/grpc++.h>

#include <memory>

namespace okts {
namespace cmds {

class CommandsDeque : virtual public Interface::Service
{
 public:
  CommandsDeque();

 private:
  grpc::Status dequePushFront(grpc::ServerContext*,
                             const PushRequest*,
                             PushReply*) final override;

  grpc::Status dequePushBack(grpc::ServerContext*,
                            const PushRequest*,
                            PushReply*) final override;

  grpc::Status dequePopFront(grpc::ServerContext*,
                            const GetValueRequest*,
                            GetValueReply*) final override;

  grpc::Status dequePopBack(grpc::ServerContext*,
                           const GetValueRequest*,
                           GetValueReply*) final override;

  grpc::Status dequeLength(grpc::ServerContext*,
                          const LengthRequest*,
                          LengthReply*) final override;

  // BLPOP
  // BRPOP
  // BRPOPLPUSH

  grpc::Status dequeIndex(grpc::ServerContext*,
                          const IndexRequest*,
                          GetValueReply*) final override;

  grpc::Status dequeInsert(grpc::ServerContext*,
                           const InsertRequest*,
                           InsertReply*) final override;

  grpc::Status dequeExistPushFront(grpc::ServerContext*,
                                   const PushRequest*,
                                   PushReply*) final override;

  grpc::Status dequeRange(grpc::ServerContext*,
                          const RangeRequest*,
                          RangeReply*) final override;

  grpc::Status dequeRemove(grpc::ServerContext*,
                          const RemoveRequest*,
                          RemoveReply*) final override;

  grpc::Status
  dequeSet(grpc::ServerContext*, const SetRequest*, SetReply*) final override;

  grpc::Status
  dequeTrim(grpc::ServerContext*, const TrimRequest*, TrimReply*) final override;

  grpc::Status dequePopBackPushFront(grpc::ServerContext*,
                                     const PopPushRequest*,
                                     PopPushReply*) final override;

  grpc::Status dequeExistPushBack(grpc::ServerContext*,
                                  const PushRequest*,
                                  PushReply*) final override;

  stor::Deques theQueues;
};

} // namespace commands
} // namespace okts
