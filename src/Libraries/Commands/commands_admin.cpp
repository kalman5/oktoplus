#include "Commands/commands_admin.h"

#include "Storage/release_memory.h"

namespace okts::cmds {

CommandsAdmin::CommandsAdmin(stor::StorageContext& aStorage)
    : theStorage(aStorage) {
}

namespace {

void flushAllStorage(stor::StorageContext& aStorage) {
  aStorage.lists.clear();
  aStorage.deques.clear();
  aStorage.vectors.clear();
  aStorage.sets.clear();
  stor::releaseMemoryToOs();
}

} // namespace

grpc::Status CommandsAdmin::flushAll(grpc::ServerContext*,
                                     const Void*,
                                     Void*) {
  flushAllStorage(theStorage);
  return grpc::Status::OK;
}

grpc::Status CommandsAdmin::flushDb(grpc::ServerContext*,
                                    const Void*,
                                    Void*) {
  // Oktoplus has a single global namespace and ignores SELECT, so
  // FLUSHDB is equivalent to FLUSHALL.
  flushAllStorage(theStorage);
  return grpc::Status::OK;
}

} // namespace okts::cmds
