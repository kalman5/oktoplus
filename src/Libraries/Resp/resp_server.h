#pragma once

#include "Storage/storage_context.h"

#include <boost/asio.hpp>

#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace okts::resp {

class Connection; // forward — defined in resp_server.cpp

// Async RESP server. Architecture:
//   - N io_contexts, each driven by exactly one thread (no strand
//     overhead, no work-migration cache thrash).
//   - Acceptor lives on io_context[0]; new connections are pinned
//     round-robin to one of the io_contexts and stay there for life.
//   - Per-connection state machine (Connection) on its owning
//     io_context: async_read_some -> drain tryParseCommand ->
//     batched async_write -> repeat.
//
// Pipeline drain no longer re-enters a blocking readCommand (closes
// PERF_TODO Q): once the parser returns NeedMore the connection
// schedules another async_read_some and returns control to the io
// loop instead of blocking on a partial frame.
class RespServer
{
 public:
  // aWorkerThreads = 0 means auto: min(hardware_concurrency, 16).
  RespServer(stor::StorageContext& aStorage,
             const std::string&    aEndpoint,
             size_t                aWorkerThreads = 0);

  ~RespServer();

  void shutdown();

 private:
  // One io_context bound to one worker thread. Workers don't share
  // io_contexts: a connection assigned to slot[i] only ever has
  // callbacks invoked from theWorkers[i].
  struct IoSlot {
    using WorkGuard = boost::asio::executor_work_guard<
        boost::asio::io_context::executor_type>;
    boost::asio::io_context io;
    WorkGuard               guard;
    std::thread             thread;

    IoSlot();
  };

  void doAccept();
  void pruneFinishedConnectionsLocked();

  stor::StorageContext&                          theStorage;
  std::vector<std::unique_ptr<IoSlot>>           theIoSlots;
  std::atomic<size_t>                            theNextSlot;
  // Acceptor lives on slot[0]'s io_context; the accept callback
  // promotes accepted sockets onto a different slot's io_context.
  // Wrapped in optional because the io_context isn't available at
  // member-init time (it's owned by a slot we construct in the body).
  std::optional<boost::asio::ip::tcp::acceptor>  theAcceptor;
  std::atomic<bool>                              theRunning;

  std::mutex                              theConnectionsMutex;
  std::list<std::weak_ptr<Connection>>    theConnections;
};

} // namespace okts::resp
