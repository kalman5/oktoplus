#pragma once

#include "Storage/storage_context.h"

#include <boost/asio.hpp>

#include <atomic>
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
  void handleConnection(boost::asio::ip::tcp::socket aSocket);

  stor::StorageContext& theStorage;
  // Synchronous-mode acceptor/resolver/socket factory — boost::asio
  // requires an io_context even when used purely synchronously.
  // run() is never called on this; the accept loop blocks in
  // theAcceptor.accept() directly.
  boost::asio::io_context        theIoContext;
  boost::asio::ip::tcp::acceptor theAcceptor;
  std::atomic<bool>              theRunning;
  std::thread                    theAcceptThread;
};

} // namespace okts::resp
