#include "Commands/commands_list.h"

#include <glog/logging.h>

#include <sstream>
#include <string_view>

namespace okts {
namespace cmds {

CommandsList::CommandsList()
    : theLists() {
}

grpc::Status CommandsList::listPushFront(grpc::ServerContext*,
                                         const PushRequest* aRequest,
                                         PushReply*         aReply) {

  std::vector<std::string_view> myStrings;
  myStrings.reserve(aRequest->values_size());
  for (int i = 0; i < aRequest->values_size(); ++i) {
    myStrings.push_back(aRequest->values(i));
  }

  const auto& myName = aRequest->name();

  auto myRet = theLists.pushFront(myName, myStrings);

  aReply->set_size(myRet);

  return grpc::Status::OK;
}

grpc::Status CommandsList::listPushBack(grpc::ServerContext*,
                                        const PushRequest* aRequest,
                                        PushReply*         aReply) {

  std::vector<std::string_view> myStrings;
  myStrings.reserve(aRequest->values_size());
  for (int i = 0; i < aRequest->values_size(); ++i) {
    myStrings.push_back(aRequest->values(i));
  }

  const auto& myName = aRequest->name();

  auto myRet = theLists.pushBack(myName, myStrings);

  aReply->set_size(myRet);

  return grpc::Status::OK;
}

grpc::Status CommandsList::listPopFront(grpc::ServerContext*,
                                        const GetValueRequest* aRequest,
                                        GetValueReply*         aReply) {

  const auto& myName = aRequest->name();

  auto myRet = theLists.popFront(myName);

  if (myRet) {
    aReply->set_value(myRet.get());
  }

  return grpc::Status::OK;
}

grpc::Status CommandsList::listPopBack(grpc::ServerContext*,
                                       const GetValueRequest* aRequest,
                                       GetValueReply*         aReply) {
  const std::string& myName = aRequest->name();

  auto myRet = theLists.popBack(myName);

  if (myRet) {
    aReply->set_value(myRet.get());
  }

  return grpc::Status::OK;
}

grpc::Status CommandsList::listLength(grpc::ServerContext*,
                                      const LengthRequest* aRequest,
                                      LengthReply*         aReply) {
  const auto& myName = aRequest->name();

  auto myRet = theLists.size(myName);

  aReply->set_value(myRet);

  return grpc::Status::OK;
}

// BLPOP
// BRPOP
// BRPOPLPUSH

grpc::Status CommandsList::listIndex(grpc::ServerContext*,
                                     const IndexRequest* aRequest,
                                     GetValueReply*      aReply) {
  const auto& myName  = aRequest->name();
  const auto  myIndex = aRequest->index();

  auto myRet = theLists.index(myName, myIndex);

  if (myRet) {
    aReply->set_value(myRet.get());
  }

  return grpc::Status::OK;
}

grpc::Status CommandsList::listInsert(grpc::ServerContext*,
                                      const InsertRequest* aRequest,
                                      InsertReply*         aReply) {
  const auto& myName     = aRequest->name();
  const auto  myPosition = aRequest->position();
  const auto& myPivot    = aRequest->pivot();
  const auto& myValue    = aRequest->value();

  stor::Lists::Position myListPosition;
  if (myPosition == InsertRequest::Position::InsertRequest_Position_BEFORE) {
    myListPosition = stor::Lists::Position::BEFORE;
  } else {
    myListPosition = stor::Lists::Position::AFTER;
  }

  auto myRet = theLists.insert(myName, myListPosition, myPivot, myValue);

  if (myRet) {
    aReply->set_size(myRet.get());
  }

  return grpc::Status::OK;
}

grpc::Status CommandsList::listExistPushFront(grpc::ServerContext*,
                                              const PushRequest* aRequest,
                                              PushReply*         aReply) {

  std::vector<std::string_view> myStrings;
  myStrings.reserve(aRequest->values_size());
  for (int i = 0; i < aRequest->values_size(); ++i) {
    myStrings.push_back(aRequest->values(i));
  }

  const auto& myName = aRequest->name();

  auto myRet = theLists.pushFrontExist(myName, myStrings);

  aReply->set_size(myRet);

  return grpc::Status::OK;
}

grpc::Status CommandsList::listRange(grpc::ServerContext*,
                                     const RangeRequest* aRequest,
                                     RangeReply*         aReply) {

  const auto& myName  = aRequest->name();
  const auto  myStart = aRequest->start();
  const auto  myStop  = aRequest->stop();

  auto myRet = theLists.range(myName, myStart, myStop);

  for (auto&& myValue : myRet) {
    aReply->add_values(std::move(myValue));
  }

  return grpc::Status::OK;
}

grpc::Status CommandsList::listRemove(grpc::ServerContext*,
                                      const RemoveRequest* aRequest,
                                      RemoveReply*         aReply) {

  const auto& myName    = aRequest->name();
  const auto  myCounter = aRequest->count();
  const auto& myValue   = aRequest->value();

  auto myRet = theLists.remove(myName, myCounter, myValue);

  aReply->set_removed(myRet);

  return grpc::Status::OK;
}

grpc::Status CommandsList::listSet(grpc::ServerContext*,
                                   const SetRequest* aRequest,
                                   SetReply*) {

  const auto& myName  = aRequest->name();
  const auto  myIndex = aRequest->index();
  const auto& myValue = aRequest->value();

  const auto myRet = theLists.set(myName, myIndex, myValue);

  switch (myRet) {
    case stor::Lists::Status::OK:
      return grpc::Status::OK;
    case stor::Lists::Status::OUT_OF_RANGE:
      return grpc::Status(grpc::OUT_OF_RANGE, "list out of range");
    case stor::Lists::Status::NOT_FOUND:
      return grpc::Status(grpc::NOT_FOUND, "list not found");
  };

  return grpc::Status(grpc::INTERNAL, "internal error");
}

grpc::Status CommandsList::listTrim(grpc::ServerContext*,
                                    const TrimRequest* aRequest,
                                    TrimReply*) {

  const auto& myName  = aRequest->name();
  const auto  myStart = aRequest->start();
  const auto  myStop  = aRequest->stop();

  theLists.trim(myName, myStart, myStop);

  return grpc::Status::OK;
}

grpc::Status CommandsList::listPopBackPushFront(grpc::ServerContext*,
                                                const PopPushRequest* aRequest,
                                                PopPushReply*         aReply) {

  const auto& mySourceName      = aRequest->source_name();
  const auto& myDestinationName = aRequest->destination_name();

  auto myRet = theLists.popBackPushFront(mySourceName, myDestinationName);

  if (myRet) {
    aReply->set_value(myRet.get());
  }

  return grpc::Status::OK;
}

grpc::Status CommandsList::listExistPushBack(grpc::ServerContext*,
                                             const PushRequest* aRequest,
                                             PushReply*         aReply) {

  std::vector<std::string_view> myStrings;
  myStrings.reserve(aRequest->values_size());
  for (int i = 0; i < aRequest->values_size(); ++i) {
    myStrings.push_back(aRequest->values(i));
  }

  const auto& myName = aRequest->name();

  auto myRet = theLists.pushBackExist(myName, myStrings);

  aReply->set_size(myRet);

  return grpc::Status::OK;
}

} // namespace cmds
} // namespace okts
