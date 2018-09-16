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
                                 const SetAddRequest*,
                                 SetAddReply*) {

  return grpc::Status::OK;
}

grpc::Status CommandsSet::setCard(grpc::ServerContext*,
                                  const SetCardRequest*,
                                  SetCardReply*) {
  return grpc::Status::OK;
}

grpc::Status CommandsSet::setDiff(grpc::ServerContext*,
                                  const SetDiffRequest*,
                                  SetDiffReply*) {
  return grpc::Status::OK;
}

} // namespace cmds
} // namespace okts
