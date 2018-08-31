#pragma once

#include "Commands/commands_server_service.h"
#include "Storage/lists.h"

#include <grpc++/grpc++.h>

#include <memory>

namespace okts {
namespace commands {

class CommandsList : virtual public Interface::Service
{
 public:
  CommandsList();

 private:
  grpc::Status listPushFront(grpc::ServerContext*,
                             const PushRequest*,
                             PushReply*) final override;

  grpc::Status listPushBack(grpc::ServerContext*,
                            const PushRequest*,
                            PushReply*) final override;

  grpc::Status listPopFront(grpc::ServerContext*,
                            const GetValueRequest*,
                            GetValueReply*) final override;

  grpc::Status listPopBack(grpc::ServerContext*,
                           const GetValueRequest*,
                           GetValueReply*) final override;

  grpc::Status listLength(grpc::ServerContext*,
                          const LengthRequest*,
                          LengthReply*) final override;

  // BLPOP
  // BRPOP
  // BRPOPLPUSH

  grpc::Status listIndex(grpc::ServerContext*,
                         const IndexRequest*,
                         GetValueReply*) final override;

  grpc::Status listInsert(grpc::ServerContext*,
                          const InsertRequest*,
                          InsertReply*) final override;

  grpc::Status listExistPushFront(grpc::ServerContext*,
                                  const PushRequest*,
                                  PushReply*) final override;

  grpc::Status listRange(grpc::ServerContext*,
                         const RangeRequest*,
                         RangeReply*) final override;

  grpc::Status listRemove(grpc::ServerContext*,
                          const RemoveRequest*,
                          RemoveReply*) final override;

  grpc::Status
  listSet(grpc::ServerContext*, const SetRequest*, SetReply*) final override;

  grpc::Status
  listTrim(grpc::ServerContext*, const TrimRequest*, TrimReply*) final override;

  grpc::Status listPopBackPushFront(grpc::ServerContext*,
                                    const PopPushRequest*,
                                    PopPushReply*) final override;

  grpc::Status listExistPushBack(grpc::ServerContext*,
                                 const PushRequest*,
                                 PushReply*) final override;

  storage::Lists theLists;
};

} // namespace commands
} // namespace okts
