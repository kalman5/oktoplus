#include "Commands/commands_deque.h"

#include <glog/logging.h>

#include <sstream>
#include <string_view>

namespace okts::cmds {

CommandsDeque::CommandsDeque()
    : theQueues() {
}

// BLMOVE
// BLMPOP
// BLPOP
// BRPOP
// BRPOPLPUSH

grpc::Status CommandsDeque::dequeIndex(grpc::ServerContext*,
                                       const IndexRequest* aRequest,
                                       GetValueReply*      aReply) {
  const auto& myName  = aRequest->name();
  const auto  myIndex = aRequest->index();

  auto myRet = theQueues.index(myName, myIndex);

  if (myRet) {
    aReply->set_value(myRet.value());
  }

  return grpc::Status::OK;
}

grpc::Status CommandsDeque::dequeInsert(grpc::ServerContext*,
                                        const InsertRequest* aRequest,
                                        InsertReply*         aReply) {
  const auto& myName     = aRequest->name();
  const auto  myPosition = aRequest->position();
  const auto& myPivot    = aRequest->pivot();
  const auto& myValue    = aRequest->value();

  stor::Deques::Position myListPosition;
  if (myPosition == InsertRequest::Position::InsertRequest_Position_BEFORE) {
    myListPosition = stor::Deques::Position::BEFORE;
  } else {
    myListPosition = stor::Deques::Position::AFTER;
  }

  auto myRet = theQueues.insert(myName, myListPosition, myPivot, myValue);

  if (myRet) {
    aReply->set_size(myRet.value());
  }

  return grpc::Status::OK;
}

grpc::Status CommandsDeque::dequeLength(grpc::ServerContext*,
                                        const LengthRequest* aRequest,
                                        LengthReply*         aReply) {
  const auto& myName = aRequest->name();

  auto myRet = theQueues.size(myName);

  aReply->set_value(myRet);

  return grpc::Status::OK;
}

grpc::Status CommandsDeque::dequeMove(grpc::ServerContext*,
                                      const MoveRequest* aRequest,
                                      GetValueReply*     aReply) {

  const auto& mySourceName           = aRequest->source_name();
  const auto& myDestinationName      = aRequest->destination_name();
  const auto  mySourceDirection      = aRequest->source_direction();
  const auto  myDestinationDirection = aRequest->destination_direction();

  auto myRet = theQueues.move(
      mySourceName,
      myDestinationName,
      mySourceDirection == MoveRequest::Direction::MoveRequest_Direction_LEFT ?
          stor::Deques::Direction::LEFT :
          stor::Deques::Direction::RIGHT,
      myDestinationDirection ==
              MoveRequest::Direction::MoveRequest_Direction_LEFT ?
          stor::Deques::Direction::LEFT :
          stor::Deques::Direction::RIGHT);

  if (myRet) {
    aReply->set_value(myRet.value());
  }

  return grpc::Status::OK;
}

// LMPOP

grpc::Status CommandsDeque::dequePopFront(grpc::ServerContext*,
                                          const PopFrontRequest* aRequest,
                                          PopFrontReply*         aReply) {

  auto myRet = theQueues.popFront(
      aRequest->name(), aRequest->has_count() ? aRequest->count().value() : 1);

  for (auto&& myValue : myRet) {
    aReply->add_value(std::move(myValue));
  }

  return grpc::Status::OK;
}

// LPOS

grpc::Status CommandsDeque::dequePushFront(grpc::ServerContext*,
                                           const PushRequest* aRequest,
                                           PushReply*         aReply) {

  std::vector<std::string_view> myStrings;
  myStrings.reserve(aRequest->values_size());
  for (int i = 0; i < aRequest->values_size(); ++i) {
    myStrings.push_back(aRequest->values(i));
  }

  const auto& myName = aRequest->name();

  auto myRet = theQueues.pushFront(myName, myStrings);

  aReply->set_size(myRet);

  return grpc::Status::OK;
}

grpc::Status CommandsDeque::dequeExistPushBack(grpc::ServerContext*,
                                               const PushRequest* aRequest,
                                               PushReply*         aReply) {

  std::vector<std::string_view> myStrings;
  myStrings.reserve(aRequest->values_size());
  for (int i = 0; i < aRequest->values_size(); ++i) {
    myStrings.push_back(aRequest->values(i));
  }

  const auto& myName = aRequest->name();

  auto myRet = theQueues.pushBackExist(myName, myStrings);

  aReply->set_size(myRet);

  return grpc::Status::OK;
}

grpc::Status CommandsDeque::dequeRange(grpc::ServerContext*,
                                       const RangeRequest* aRequest,
                                       RangeReply*         aReply) {

  const auto& myName  = aRequest->name();
  const auto  myStart = aRequest->start();
  const auto  myStop  = aRequest->stop();

  auto myRet = theQueues.range(myName, myStart, myStop);

  for (auto&& myValue : myRet) {
    aReply->add_values(std::move(myValue));
  }

  return grpc::Status::OK;
}

grpc::Status CommandsDeque::dequeRemove(grpc::ServerContext*,
                                        const RemoveRequest* aRequest,
                                        RemoveReply*         aReply) {

  const auto& myName    = aRequest->name();
  const auto  myCounter = aRequest->count();
  const auto& myValue   = aRequest->value();

  auto myRet = theQueues.remove(myName, myCounter, myValue);

  aReply->set_removed(myRet);

  return grpc::Status::OK;
}

grpc::Status CommandsDeque::dequeSet(grpc::ServerContext*,
                                     const SetRequest* aRequest,
                                     SetReply*) {

  const auto& myName  = aRequest->name();
  const auto  myIndex = aRequest->index();
  const auto& myValue = aRequest->value();

  const auto myRet = theQueues.set(myName, myIndex, myValue);

  switch (myRet) {
    case stor::Deques::Status::OK:
      return grpc::Status::OK;
    case stor::Deques::Status::OUT_OF_RANGE:
      return grpc::Status(grpc::OUT_OF_RANGE, "list out of range");
    case stor::Deques::Status::NOT_FOUND:
      return grpc::Status(grpc::NOT_FOUND, "list not found");
  };

  return grpc::Status(grpc::INTERNAL, "internal error");
}

grpc::Status CommandsDeque::dequeTrim(grpc::ServerContext*,
                                      const TrimRequest* aRequest,
                                      TrimReply*) {

  const auto& myName  = aRequest->name();
  const auto  myStart = aRequest->start();
  const auto  myStop  = aRequest->stop();

  theQueues.trim(myName, myStart, myStop);

  return grpc::Status::OK;
}

grpc::Status CommandsDeque::dequePopBack(grpc::ServerContext*,
                                         const PopBackRequest* aRequest,
                                         PopBackReply*         aReply) {
  auto myRet = theQueues.popBack(
      aRequest->name(), aRequest->has_count() ? aRequest->count().value() : 1);

  for (auto&& myValue : myRet) {
    aReply->add_value(std::move(myValue));
  }

  return grpc::Status::OK;
}

grpc::Status CommandsDeque::dequePushBack(grpc::ServerContext*,
                                          const PushRequest* aRequest,
                                          PushReply*         aReply) {

  std::vector<std::string_view> myStrings;
  myStrings.reserve(aRequest->values_size());
  for (int i = 0; i < aRequest->values_size(); ++i) {
    myStrings.push_back(aRequest->values(i));
  }

  const auto& myName = aRequest->name();

  auto myRet = theQueues.pushBack(myName, myStrings);

  aReply->set_size(myRet);

  return grpc::Status::OK;
}

grpc::Status CommandsDeque::dequeExistPushFront(grpc::ServerContext*,
                                                const PushRequest* aRequest,
                                                PushReply*         aReply) {

  std::vector<std::string_view> myStrings;
  myStrings.reserve(aRequest->values_size());
  for (int i = 0; i < aRequest->values_size(); ++i) {
    myStrings.push_back(aRequest->values(i));
  }

  const auto& myName = aRequest->name();

  auto myRet = theQueues.pushFrontExist(myName, myStrings);

  aReply->set_size(myRet);

  return grpc::Status::OK;
}

} // namespace okts::cmds
