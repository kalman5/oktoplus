#include "Commands/commands_set.h"

#include <glog/logging.h>

#include <sstream>
#include <string_view>

namespace okts {
namespace cmds {

CommandsSet::CommandsSet()
    : theSets() {
}

grpc::Status CommandsSet::setAdd(grpc::ServerContext*,
                                 const SetAddRequest* aRequest,
                                 SetAddReply*         aReply) {

  std::vector<std::string_view> myStrings;
  myStrings.reserve(aRequest->values_size());
  for (int i = 0; i < aRequest->values_size(); ++i) {
    myStrings.push_back(aRequest->values(i));
  }

  const auto& myName = aRequest->name();

  auto myRet = theSets.add(myName, myStrings);

  aReply->set_size(myRet);

  return grpc::Status::OK;
}

grpc::Status CommandsSet::setCard(grpc::ServerContext*,
                                  const SetCardRequest* aRequest,
                                  SetCardReply*         aReply) {

  const auto& myName = aRequest->name();

  auto myRet = theSets.cardinality(myName);

  aReply->set_size(myRet);

  return grpc::Status::OK;
}

grpc::Status CommandsSet::setDiff(grpc::ServerContext*,
                                  const SetDiffRequest* aRequest,
                                  SetDiffReply*         aReply) {

  std::vector<std::string_view> myNames;
  myNames.reserve(aRequest->values_size());
  for (int i = 0; i < aRequest->values_size(); ++i) {
    myNames.push_back(aRequest->values(i));
  }

  auto myRet = theSets.diff(myNames);

  for (auto const& myValue : myRet) {
    aReply->add_values(myValue);
  }

  return grpc::Status::OK;
}

} // namespace cmds
} // namespace okts
