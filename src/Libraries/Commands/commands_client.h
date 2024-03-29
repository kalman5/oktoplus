#pragma once

#include "Commands/commands_server_service.h"
#include "Storage/sequencecontainer.h"

#include <cstdint>
#include <grpc++/grpc++.h>

#include <memory>

namespace okts::cmds {

class CommandsClient final
{
 public:
  CommandsClient(const std::string& aEndpoint);

  size_t listPushFront(const std::string&              aContainerName,
                       const std::vector<std::string>& aValues);
  size_t dequePushFront(const std::string&              aContainerName,
                        const std::vector<std::string>& aValues);

  void
  listTrim(const std::string& aContainerName, int64_t aStart, int64_t aStop);
  void
  dequeTrim(const std::string& aContainerName, int64_t aStart, int64_t aStop);

  std::list<std::string> listPopFront(const std::string& aContainerName,
                                      uint64_t           aCount);
  std::list<std::string> dequePopFront(const std::string& aContainerName,
                                       uint64_t           aCount);

  size_t listLength(const std::string& aContainerName);
  size_t dequeLength(const std::string& aContainerName);

 private:
  size_t pushFront(
      const std::string&              aContainerName,
      const std::vector<std::string>& aValues,
      const std::function<::grpc::Status(
          ::grpc::ClientContext*, const PushRequest&, PushReply*)>& aFunction);

  void
  trim(const std::string& aContainerName,
       int64_t            aStart,
       int64_t            aStop,
       const std::function<::grpc::Status(
           ::grpc::ClientContext*, const TrimRequest&, TrimReply*)>& aFunction);

  std::list<std::string>
  popFront(const std::string&                                   aContainerName,
           uint64_t                                             aCount,
           const std::function<::grpc::Status(::grpc::ClientContext*,
                                              const PopFrontRequest&,
                                              PopFrontReply*)>& aFunction);

  size_t length(const std::string& aContainerName,
                const std::function<::grpc::Status(::grpc::ClientContext*,
                                                   const LengthRequest&,
                                                   LengthReply*)>& aFunction);

  std::shared_ptr<::grpc::ChannelCredentials> theCredentials;

  std::unique_ptr<Interface::Stub> theStub;
};

} // namespace okts::cmds