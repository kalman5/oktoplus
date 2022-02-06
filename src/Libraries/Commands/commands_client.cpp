#include "Commands/commands_client.h"

#include <glog/logging.h>

#include <sstream>
#include <string_view>

namespace okts::cmds {

CommandsClient::CommandsClient(const std::string& aEndpoint)
    : theCredentials(::grpc::InsecureChannelCredentials())
    , theStub(Interface::NewStub(
          ::grpc::CreateChannel(aEndpoint, theCredentials))) {
}

size_t CommandsClient::listPushFront(const std::string&         aListName,
                                     std::vector<std::string>&& aValues) {

  const std::chrono::system_clock::time_point myDeadline =
      std::chrono::system_clock::now() + std::chrono::seconds(5);

  ::grpc::ClientContext myContext;
  myContext.set_deadline(myDeadline);

  PushRequest myRequest;
  myRequest.set_name(aListName);

  for (auto&& myValue : aValues) {
    myRequest.add_values(std::move(myValue));
  }

  PushReply myReply;

  ::grpc::Status myStatus =
      theStub->listPushFront(&myContext, myRequest, &myReply);

  if (not myStatus.ok()) {
    throw std::runtime_error(myStatus.error_message());
  }

  return myReply.size();
}

void CommandsClient::listTrim(const std::string& aListName,
                              int64_t            aStart,
                              int64_t            aStop) {
  const std::chrono::system_clock::time_point myDeadline =
      std::chrono::system_clock::now() + std::chrono::seconds(5);

  ::grpc::ClientContext myContext;
  myContext.set_deadline(myDeadline);

  TrimRequest myRequest;
  myRequest.set_name(aListName);
  myRequest.set_start(aStart);
  myRequest.set_stop(aStop);

  TrimReply myReply;

  ::grpc::Status myStatus = theStub->listTrim(&myContext, myRequest, &myReply);

  if (not myStatus.ok()) {
    throw std::runtime_error(myStatus.error_message());
  }
}

std::string CommandsClient::listPopFront(const std::string& aListName) {

  GetValueRequest myRequest;

  myRequest.set_name(aListName);

  std::chrono::system_clock::time_point deadline =
      std::chrono::system_clock::now() + std::chrono::seconds(5);

  ::grpc::ClientContext myContext;
  myContext.set_deadline(deadline);

  GetValueReply myReply;

  ::grpc::Status myStatus =
      theStub->listPopFront(&myContext, myRequest, &myReply);

  if (not myStatus.ok()) {
    throw std::runtime_error(myStatus.error_message());
  }

  return myReply.value();
}

} // namespace okts::cmds