#pragma once

#include "Storage/storage_context.h"

#include <boost/asio.hpp>

#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace okts::resp {

class RespServer
{
 public:
  RespServer(stor::StorageContext& aStorage, const std::string& aEndpoint);

  ~RespServer();

  void shutdown();

 private:
  void acceptLoop();
  void handleConnection(
      std::shared_ptr<boost::asio::ip::tcp::socket> aSocket);

  // Per-active-connection record. socket is shared with the worker
  // thread so shutdown() can close it from outside to unblock a
  // blocked read; worker is the std::thread driving handleConnection.
  struct Connection {
    std::shared_ptr<boost::asio::ip::tcp::socket> socket;
    std::thread                                   worker;
  };

  // Drop entries whose worker has already finished. Called under
  // theConnectionsMutex from the accept path so the list doesn't
  // grow unboundedly when long-lived clients come and go.
  void pruneFinishedConnectionsLocked();

  stor::StorageContext& theStorage;
  // Synchronous-mode acceptor/resolver/socket factory — boost::asio
  // requires an io_context even when used purely synchronously.
  // run() is never called on this; the accept loop blocks in
  // theAcceptor.accept() directly.
  boost::asio::io_context        theIoContext;
  boost::asio::ip::tcp::acceptor theAcceptor;
  std::atomic<bool>              theRunning;
  std::thread                    theAcceptThread;

  // Live client connections. shutdown() walks this list, closes every
  // socket to wake blocked workers, then joins each worker. Replaces
  // the previous detached-thread model where workers could outlive
  // the server (and StorageContext) and trigger UAF.
  std::mutex            theConnectionsMutex;
  std::list<Connection> theConnections;
};

} // namespace okts::resp
