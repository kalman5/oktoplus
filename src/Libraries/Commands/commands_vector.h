#pragma once

#include "Commands/commands_server_service.h"
#include "Storage/vectors.h"

#include <grpc++/grpc++.h>

#include <memory>

namespace oktoplus {
namespace commands {

class CommandsVector : virtual public Interface::Service
{
 public:
  CommandsVector();

 private:
  grpc::Status vectorPushBack(grpc::ServerContext*,
                            const PushRequest*,
                            PushReply*) final override;

  grpc::Status vectorPopBack(grpc::ServerContext*,
                           const GetValueRequest*,
                           GetValueReply*) final override;

  grpc::Status vectorLength(grpc::ServerContext*,
                          const LengthRequest*,
                          LengthReply*) final override;

  // BLPOP
  // BRPOP
  // BRPOPLPUSH

  grpc::Status vectorIndex(grpc::ServerContext*,
                         const IndexRequest*,
                         GetValueReply*) final override;

  grpc::Status vectorInsert(grpc::ServerContext*,
                          const InsertRequest*,
                          InsertReply*) final override;

  grpc::Status vectorRange(grpc::ServerContext*,
                         const RangeRequest*,
                         RangeReply*) final override;

  grpc::Status vectorRemove(grpc::ServerContext*,
                          const RemoveRequest*,
                          RemoveReply*) final override;

  grpc::Status
  vectorSet(grpc::ServerContext*, const SetRequest*, SetReply*) final override;

  grpc::Status
  vectorTrim(grpc::ServerContext*, const TrimRequest*, TrimReply*) final override;

  grpc::Status vectorExistPushBack(grpc::ServerContext*,
                                 const PushRequest*,
                                 PushReply*) final override;

  storage::Vectors theVectors;
};

} // namespace commands
} // namespace oktoplus
