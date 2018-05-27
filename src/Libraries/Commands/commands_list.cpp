#include "Commands/commands_list.h"

#include <glog/logging.h>

#include <sstream>
#include <string_view>

namespace oktoplus {
namespace commands {

CommandsList::CommandsList()
    : theLists() {
}

grpc::Status CommandsList::listPushFront(grpc::ServerContext*,
                                         const ListPushRequest* aRequest,
                                         ListPushReply*         aReply) {

  std::vector<std::string_view> myStrings;
  myStrings.reserve(aRequest->values_size());
  for (int i = 0; i < aRequest->values_size(); ++i) {
    myStrings.push_back(aRequest->values(i));
  }

  const auto& myName = aRequest->list_name();

  auto myRet = theLists.pushFront(myName, myStrings);

  aReply->set_size(myRet);

  return grpc::Status::OK;
}

grpc::Status CommandsList::listPushBack(grpc::ServerContext*,
                                        const ListPushRequest* aRequest,
                                        ListPushReply*         aReply) {

  std::vector<std::string_view> myStrings;
  myStrings.reserve(aRequest->values_size());
  for (int i = 0; i < aRequest->values_size(); ++i) {
    myStrings.push_back(aRequest->values(i));
  }

  const auto& myName = aRequest->list_name();

  auto myRet = theLists.pushBack(myName, myStrings);

  aReply->set_size(myRet);

  return grpc::Status::OK;
}

grpc::Status CommandsList::listPopFront(grpc::ServerContext*,
                                        const ListGetValueRequest* aRequest,
                                        ListGetValueReply*         aReply) {

  const auto& myName = aRequest->list_name();

  auto myRet = theLists.popFront(myName);

  if (myRet) {
    aReply->set_value(myRet.get());
  }

  return grpc::Status::OK;
}

grpc::Status CommandsList::listPopBack(grpc::ServerContext*,
                                       const ListGetValueRequest* aRequest,
                                       ListGetValueReply*         aReply) {
  const std::string& myName = aRequest->list_name();

  auto myRet = theLists.popBack(myName);

  if (myRet) {
    aReply->set_value(myRet.get());
  }

  return grpc::Status::OK;
}

grpc::Status CommandsList::listLength(grpc::ServerContext*,
                                      const ListLengthRequest* aRequest,
                                      ListLengthReply*         aReply) {
  const auto& myName = aRequest->list_name();

  auto myRet = theLists.size(myName);

  aReply->set_value(myRet);

  return grpc::Status::OK;
}

// BLPOP
// BRPOP
// BRPOPLPUSH

grpc::Status CommandsList::listEntryAtIndex(grpc::ServerContext*,
                                            const IndexRequest* aRequest,
                                            ListGetValueReply*  aReply) {
  const auto& myName  = aRequest->name();
  const auto  myIndex = aRequest->index();

  auto myRet = theLists.index(myName, myIndex);

  if (myRet) {
    aReply->set_value(myRet.get());
  }

  return grpc::Status::OK;
}

grpc::Status CommandsList::listInsert(grpc::ServerContext*,
                                      const ListInsertRequest* aRequest,
                                      ListInsertReply*         aReply) {
  const auto& myName     = aRequest->list_name();
  const auto  myPosition = aRequest->position();
  const auto& myPivot    = aRequest->pivot();
  const auto& myValue    = aRequest->value();

  storage::Lists::Position myListPosition;
  if (myPosition ==
      ListInsertRequest::Position::ListInsertRequest_Position_BEFORE) {
    myListPosition = storage::Lists::Position::BEFORE;
  } else {
    myListPosition = storage::Lists::Position::AFTER;
  }

  auto myRet = theLists.insert(myName, myListPosition, myPivot, myValue);

  if (myRet) {
    aReply->set_size(myRet.get());
  }

  return grpc::Status::OK;
}

grpc::Status CommandsList::listRange(grpc::ServerContext*,
                                     const RangeRequest* aRequest,
                                     RangeReply*         aReply) {

  const auto& myName  = aRequest->list_name();
  const auto  myStart = aRequest->start();
  const auto  myStop  = aRequest->stop();

  auto myRet = theLists.range(myName, myStart, myStop);

  for (auto&& myValue : myRet) {
    aReply->add_values(std::move(myValue));
  }

  return grpc::Status::OK;
}

} // namespace commands
} // namespace oktoplus
