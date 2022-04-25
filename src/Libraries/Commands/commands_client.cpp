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

size_t CommandsClient::listPushFront(const std::string& aContainerName,
                                     const std::vector<std::string>& aValues) {

  return pushFront(aContainerName,
                   aValues,
                   [this](::grpc::ClientContext* aContext,
                          const PushRequest&     aRequest,
                          PushReply*             aReply) {
                     return theStub->listPushFront(aContext, aRequest, aReply);
                   });
}

size_t CommandsClient::dequePushFront(const std::string& aContainerName,
                                      const std::vector<std::string>& aValues) {

  return pushFront(aContainerName,
                   aValues,
                   [this](::grpc::ClientContext* aContext,
                          const PushRequest&     aRequest,
                          PushReply*             aReply) {
                     return theStub->dequePushFront(aContext, aRequest, aReply);
                   });
}

void CommandsClient::listTrim(const std::string& aListName,
                              int64_t            aStart,
                              int64_t            aStop) {

  trim(aListName,
       aStart,
       aStop,
       [this](::grpc::ClientContext* aContext,
              const TrimRequest&     aRequest,
              TrimReply*             aReply) {
         return theStub->listTrim(aContext, aRequest, aReply);
       });
}

void CommandsClient::dequeTrim(const std::string& aListName,
                               int64_t            aStart,
                               int64_t            aStop) {
  trim(aListName,
       aStart,
       aStop,
       [this](::grpc::ClientContext* aContext,
              const TrimRequest&     aRequest,
              TrimReply*             aReply) {
         return theStub->dequeTrim(aContext, aRequest, aReply);
       });
}

std::list<std::string>
CommandsClient::listPopFront(const std::string& aContainerName,
                             const uint64_t     aCount) {
  return popFront(aContainerName,
                  aCount,
                  [this](::grpc::ClientContext* aContext,
                         const PopFrontRequest& aRequest,
                         PopFrontReply*         aReply) {
                    return theStub->listPopFront(aContext, aRequest, aReply);
                  });
}

std::list<std::string>
CommandsClient::dequePopFront(const std::string& aContainerName,
                              const uint64_t     aCount) {
  return popFront(aContainerName,
                  aCount,
                  [this](::grpc::ClientContext* aContext,
                         const PopFrontRequest& aRequest,
                         PopFrontReply*         aReply) {
                    return theStub->dequePopFront(aContext, aRequest, aReply);
                  });
}

size_t CommandsClient::listLength(const std::string& aContainerName) {
  return length(aContainerName,
                [this](::grpc::ClientContext* aContext,
                       const LengthRequest&   aRequest,
                       LengthReply*           aReply) {
                  return theStub->listLength(aContext, aRequest, aReply);
                });
}

size_t CommandsClient::dequeLength(const std::string& aContainerName) {
  return length(aContainerName,
                [this](::grpc::ClientContext* aContext,
                       const LengthRequest&   aRequest,
                       LengthReply*           aReply) {
                  return theStub->dequeLength(aContext, aRequest, aReply);
                });
}

////

size_t CommandsClient::pushFront(
    const std::string&              aContainerName,
    const std::vector<std::string>& aValues,
    const std::function<::grpc::Status(
        ::grpc::ClientContext*, const PushRequest&, PushReply*)>& aFunction) {
  const std::chrono::system_clock::time_point myDeadline =
      std::chrono::system_clock::now() + std::chrono::seconds(5);

  ::grpc::ClientContext myContext;
  myContext.set_deadline(myDeadline);

  PushRequest myRequest;
  myRequest.set_name(aContainerName);

  for (auto&& myValue : aValues) {
    myRequest.add_values(myValue);
  }

  PushReply myReply;

  ::grpc::Status myStatus = aFunction(&myContext, myRequest, &myReply);

  if (not myStatus.ok()) {
    throw std::runtime_error(myStatus.error_message());
  }

  return myReply.size();
}

void CommandsClient::trim(
    const std::string& aContainerName,
    int64_t            aStart,
    int64_t            aStop,
    const std::function<::grpc::Status(
        ::grpc::ClientContext*, const TrimRequest&, TrimReply*)>& aFunction) {
  const std::chrono::system_clock::time_point myDeadline =
      std::chrono::system_clock::now() + std::chrono::seconds(5);

  ::grpc::ClientContext myContext;
  myContext.set_deadline(myDeadline);

  TrimRequest myRequest;
  myRequest.set_name(aContainerName);
  myRequest.set_start(aStart);
  myRequest.set_stop(aStop);

  TrimReply myReply;

  ::grpc::Status myStatus = aFunction(&myContext, myRequest, &myReply);

  if (not myStatus.ok()) {
    throw std::runtime_error(myStatus.error_message());
  }
}

std::list<std::string> CommandsClient::popFront(
    const std::string&                                   aContainerName,
    const uint64_t                                       aCount,
    const std::function<::grpc::Status(::grpc::ClientContext*,
                                       const PopFrontRequest&,
                                       PopFrontReply*)>& aFunction) {

  PopFrontRequest myRequest;
  myRequest.set_name(aContainerName);
  myRequest.mutable_count()->set_value(aCount);

  std::chrono::system_clock::time_point deadline =
      std::chrono::system_clock::now() + std::chrono::seconds(5);

  ::grpc::ClientContext myContext;
  myContext.set_deadline(deadline);

  PopFrontReply myReply;

  const ::grpc::Status myStatus = aFunction(&myContext, myRequest, &myReply);

  if (not myStatus.ok()) {
    throw std::runtime_error(myStatus.error_message());
  }

  std::list<std::string> myRet;
  for (auto myValue : myReply.value()) {
    myRet.emplace_back(std::move(myValue));
  }

  return myRet;
}

size_t CommandsClient::length(
    const std::string&                                 aContainerName,
    const std::function<::grpc::Status(::grpc::ClientContext*,
                                       const LengthRequest&,
                                       LengthReply*)>& aFunction) {
  LengthRequest myRequest;

  myRequest.set_name(aContainerName);

  std::chrono::system_clock::time_point deadline =
      std::chrono::system_clock::now() + std::chrono::seconds(5);

  ::grpc::ClientContext myContext;
  myContext.set_deadline(deadline);

  LengthReply myReply;

  ::grpc::Status myStatus = aFunction(&myContext, myRequest, &myReply);

  if (not myStatus.ok()) {
    throw std::runtime_error(myStatus.error_message());
  }

  return myReply.value();
}

} // namespace okts::cmds