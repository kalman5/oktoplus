#pragma once

#include "Commands/commands_server_service.h"
#include "Storage/lists.h"

#include <grpc++/grpc++.h>

#include <memory>

namespace okts::cmds {

class CommandsClient final
{
 public:
  CommandsClient(const std::string& aEndpoint);

  size_t listPushFront(const std::string&         aListName,
                       std::vector<std::string>&& aValues);

  void listTrim(const std::string& aListName, int64_t aStart, int64_t aStop);

  std::string listPopFront(const std::string& aListName);

 private:
  std::shared_ptr<::grpc::ChannelCredentials> theCredentials;

  std::unique_ptr<Interface::Stub> theStub;
};

} // namespace okts::cmds