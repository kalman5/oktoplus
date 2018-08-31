#include "Commands/commands_client.h"

#include <glog/logging.h>

#include <sstream>
#include <string_view>

namespace okts {
namespace commands {

CommandsClient::CommandsClient(const std::string& myEndpoint)
    : theCredentials(::grpc::InsecureChannelCredentials())
    , theStub(Interface::NewStub(
          ::grpc::CreateChannel(myEndpoint, theCredentials))) {
}

size_t CommandsClient::listPushFront(const std::string&             aListName,
                                     const std::vector<std::string> aValues) {

  PushRequest myRequest;

  myRequest.set_name(aListName);

  for (const auto& myValue : aValues) {
    myRequest.add_values(std::move(myValue));
  }

  std::chrono::system_clock::time_point deadline =
      std::chrono::system_clock::now() + std::chrono::seconds(5);

  ::grpc::ClientContext myContext;
  myContext.set_deadline(deadline);

  PushReply myReply;

  ::grpc::Status myStatus =
      theStub->listPushFront(&myContext, myRequest, &myReply);

  if (not myStatus.ok()) {
    throw std::runtime_error(myStatus.error_message());
  }

  return myReply.size();
}

// grpc::Status CommandsServer::listPushBack(grpc::ServerContext*,
//                                           const ListPushRequest* aRequest,
//                                           ListPushReply*         aReply) {
//
//   std::vector<std::string_view> myStrings;
//   myStrings.reserve(aRequest->values_size());
//   for (int i = 0; i < aRequest->values_size(); ++i) {
//     myStrings.push_back(aRequest->values(i));
//   }
//
//   const std::string& myName = aRequest->list_name();
//
//   auto myRet = theLists.pushBack(myName, myStrings);
//
//   aReply->set_size(myRet);
//
//   return grpc::Status::OK;
// }

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

// grpc::Status CommandsServer::listPopBack(grpc::ServerContext*,
//                                          const ListPopRequest* aRequest,
//                                          ListPopReply*         aReply) {
//   const std::string& myName = aRequest->list_name();
//
//   auto myRet = theLists.popBack(myName);
//
//   if (myRet) {
//     aReply->set_value(myRet.get());
//   }
//
//   return grpc::Status::OK;
// }

} // namespace commands
} // namespace okts
