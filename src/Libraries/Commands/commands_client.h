#pragma once

#include "Commands/commands_server_service.h"
#include "Storage/lists.h"

#include <grpc++/grpc++.h>

#include <memory>

namespace oktoplus {
namespace commands {

class CommandsClient final
{
 public:
  CommandsClient(const std::string& myEndpoint);

  size_t listPushFront(const std::string&             myListName,
                       const std::vector<std::string> myValues);

//   grpc::Status listPushBack(grpc::ServerContext*,
//                             const ListPushRequest*,
//                             ListPushReply*) override;
//

  std::string listPopFront(const std::string& myListName);
//
//   grpc::Status listPopBack(grpc::ServerContext*,
//                            const ListPopRequest*,
//                            ListPopReply*) override;
//

  std::shared_ptr<::grpc::ChannelCredentials> theCredentials;

  std::unique_ptr<Interface::Stub> theStub;
};

} // namespace commands
} // namespace octoplus
