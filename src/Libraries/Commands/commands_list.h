#pragma once

#include "Commands/commands_server_service.h"
#include "Storage/lists.h"

#include <grpc++/grpc++.h>

#include <memory>

namespace oktoplus {
namespace commands {

class CommandsList : virtual public Interface::Service
{
 public:
  CommandsList();

 private:
  grpc::Status listPushFront(grpc::ServerContext*,
                             const ListPushRequest*,
                             ListPushReply*) final override;

  grpc::Status listPushBack(grpc::ServerContext*,
                            const ListPushRequest*,
                            ListPushReply*) final override;

  grpc::Status listPopFront(grpc::ServerContext*,
                            const ListGetValueRequest*,
                            ListGetValueReply*) final override;

  grpc::Status listPopBack(grpc::ServerContext*,
                           const ListGetValueRequest*,
                           ListGetValueReply*) final override;

  grpc::Status listLength(grpc::ServerContext*,
                          const ListLengthRequest*,
                          ListLengthReply*) final override;

  // BLPOP
  // BRPOP
  // BRPOPLPUSH

  grpc::Status listIndex(grpc::ServerContext*,
                         const IndexRequest*,
                         ListGetValueReply*) final override;

  grpc::Status listInsert(grpc::ServerContext*,
                          const ListInsertRequest*,
                          ListInsertReply*) final override;

  grpc::Status listExistPushFront(grpc::ServerContext*,
                                  const ListPushRequest*,
                                  ListPushReply*) final override;

  grpc::Status listRange(grpc::ServerContext*,
                         const RangeRequest*,
                         RangeReply*) final override;

  grpc::Status listRemove(grpc::ServerContext*,
                          const RemoveRequest*,
                          RemoveReply*) final override;

  grpc::Status listSet(grpc::ServerContext*,
                       const SetRequest*,
                       SetReply*) final override;

  grpc::Status listTrim(grpc::ServerContext*,
                       const TrimRequest*,
                       TrimReply*) final override;

  // RPOPLPUSH

  grpc::Status listExistPushBack(grpc::ServerContext*,
                                 const ListPushRequest*,
                                 ListPushReply*) final override;

  storage::Lists theLists;
};

} // namespace commands
} // namespace oktoplus
