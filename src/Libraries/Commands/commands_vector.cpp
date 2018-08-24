#include "Commands/commands_vector.h"

#include <glog/logging.h>

#include <sstream>
#include <string_view>

namespace oktoplus {
namespace commands {

CommandsVector::CommandsVector()
    : theVectors() {
}

grpc::Status CommandsVector::vectorPushBack(grpc::ServerContext*,
                                        const PushRequest* aRequest,
                                        PushReply*         aReply) {

  std::vector<std::string_view> myStrings;
  myStrings.reserve(aRequest->values_size());
  for (int i = 0; i < aRequest->values_size(); ++i) {
    myStrings.push_back(aRequest->values(i));
  }

  const auto& myName = aRequest->name();

  auto myRet = theVectors.pushBack(myName, myStrings);

  aReply->set_size(myRet);

  return grpc::Status::OK;
}

grpc::Status CommandsVector::vectorPopBack(grpc::ServerContext*,
                                       const GetValueRequest* aRequest,
                                       GetValueReply*         aReply) {
  const std::string& myName = aRequest->name();

  auto myRet = theVectors.popBack(myName);

  if (myRet) {
    aReply->set_value(myRet.get());
  }

  return grpc::Status::OK;
}

grpc::Status CommandsVector::vectorLength(grpc::ServerContext*,
                                      const LengthRequest* aRequest,
                                      LengthReply*         aReply) {
  const auto& myName = aRequest->name();

  auto myRet = theVectors.size(myName);

  aReply->set_value(myRet);

  return grpc::Status::OK;
}

// BLPOP
// BRPOP
// BRPOPLPUSH

grpc::Status CommandsVector::vectorIndex(grpc::ServerContext*,
                                     const IndexRequest* aRequest,
                                     GetValueReply*  aReply) {
  const auto& myName  = aRequest->name();
  const auto  myIndex = aRequest->index();

  auto myRet = theVectors.index(myName, myIndex);

  if (myRet) {
    aReply->set_value(myRet.get());
  }

  return grpc::Status::OK;
}

grpc::Status CommandsVector::vectorInsert(grpc::ServerContext*,
                                      const InsertRequest* aRequest,
                                      InsertReply*         aReply) {
  const auto& myName     = aRequest->name();
  const auto  myPosition = aRequest->position();
  const auto& myPivot    = aRequest->pivot();
  const auto& myValue    = aRequest->value();

  storage::Vectors::Position myListPosition;
  if (myPosition ==
      InsertRequest::Position::InsertRequest_Position_BEFORE) {
    myListPosition = storage::Vectors::Position::BEFORE;
  } else {
    myListPosition = storage::Vectors::Position::AFTER;
  }

  auto myRet = theVectors.insert(myName, myListPosition, myPivot, myValue);

  if (myRet) {
    aReply->set_size(myRet.get());
  }

  return grpc::Status::OK;
}

grpc::Status CommandsVector::vectorRange(grpc::ServerContext*,
                                     const RangeRequest* aRequest,
                                     RangeReply*         aReply) {

  const auto& myName  = aRequest->name();
  const auto  myStart = aRequest->start();
  const auto  myStop  = aRequest->stop();

  auto myRet = theVectors.range(myName, myStart, myStop);

  for (auto&& myValue : myRet) {
    aReply->add_values(std::move(myValue));
  }

  return grpc::Status::OK;
}

grpc::Status CommandsVector::vectorRemove(grpc::ServerContext*,
                                      const RemoveRequest* aRequest,
                                      RemoveReply*         aReply) {

  const auto& myName    = aRequest->name();
  const auto  myCounter = aRequest->count();
  const auto& myValue   = aRequest->value();

  auto myRet = theVectors.remove(myName, myCounter, myValue);

  aReply->set_removed(myRet);

  return grpc::Status::OK;
}

grpc::Status CommandsVector::vectorSet(grpc::ServerContext*,
                                   const SetRequest* aRequest,
                                   SetReply*) {

  const auto& myName  = aRequest->name();
  const auto  myIndex = aRequest->index();
  const auto& myValue = aRequest->value();

  const auto myRet = theVectors.set(myName, myIndex, myValue);

  switch (myRet) {
    case storage::Vectors::Status::OK:
      return grpc::Status::OK;
    case storage::Vectors::Status::OUT_OF_RANGE:
      return grpc::Status(grpc::OUT_OF_RANGE, "list out of range");
    case storage::Vectors::Status::NOT_FOUND:
      return grpc::Status(grpc::NOT_FOUND, "list not found");
  };
}

grpc::Status CommandsVector::vectorTrim(grpc::ServerContext*,
                                    const TrimRequest* aRequest,
                                    TrimReply*) {

  const auto& myName  = aRequest->name();
  const auto  myStart = aRequest->start();
  const auto  myStop  = aRequest->stop();

  theVectors.trim(myName, myStart, myStop);

  return grpc::Status::OK;
}

grpc::Status CommandsVector::vectorExistPushBack(grpc::ServerContext*,
                                             const PushRequest* aRequest,
                                             PushReply*         aReply) {

  std::vector<std::string_view> myStrings;
  myStrings.reserve(aRequest->values_size());
  for (int i = 0; i < aRequest->values_size(); ++i) {
    myStrings.push_back(aRequest->values(i));
  }

  const auto& myName = aRequest->name();

  auto myRet = theVectors.pushBackExist(myName, myStrings);

  aReply->set_size(myRet);

  return grpc::Status::OK;
}

} // namespace commands
} // namespace oktoplus
