#include "Commands/commands_admin.h"

namespace okts::cmds {

CommandsAdmin::CommandsAdmin(stor::StorageContext& aStorage)
    : theStorage(aStorage) {
}

namespace {

// Drop every container; do NOT call jemalloc's purge. Asking the
// allocator to give pages back to the OS is a separate concern; the
// RESP path exposes it as `MEMORY PURGE`. The gRPC interface has no
// equivalent today -- callers that need it should issue MEMORY PURGE
// over RESP, or wait for jemalloc's background dirty-page decay.
void flushAllStorage(stor::StorageContext& aStorage) {
  aStorage.lists.clear();
  aStorage.deques.clear();
  aStorage.vectors.clear();
  aStorage.sets.clear();
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
