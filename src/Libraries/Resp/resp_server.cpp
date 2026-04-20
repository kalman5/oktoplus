#include "Resp/resp_server.h"
#include "Resp/resp_handler.h"
#include "Resp/resp_parser.h"

#include <glog/logging.h>

#include <algorithm>
#include <cctype>
#include <chrono>

namespace okts::resp {

namespace {

std::pair<std::string, uint16_t> parseEndpoint(const std::string& aEndpoint) {
  auto myPos = aEndpoint.rfind(':');
  if (myPos == std::string::npos) {
    throw std::runtime_error(
        "Invalid RESP endpoint format, expected host:port");
  }
  return {aEndpoint.substr(0, myPos),
          static_cast<uint16_t>(std::stoi(aEndpoint.substr(myPos + 1)))};
}

std::string toUpper(std::string aStr) {
  std::transform(
      aStr.begin(), aStr.end(), aStr.begin(), [](unsigned char c) {
        return std::toupper(c);
      });
  return aStr;
}

} // namespace

RespServer::RespServer(stor::StorageContext& aStorage,
                       const std::string&    aEndpoint)
    : theStorage(aStorage)
    , theIoContext()
    , theAcceptor(theIoContext)
    , theRunning(true)
    , theAcceptThread() {

  auto [myHost, myPort] = parseEndpoint(aEndpoint);

  boost::asio::ip::tcp::resolver myResolver(theIoContext);
  auto myResults = myResolver.resolve(myHost, std::to_string(myPort));

  auto myEndpoint = myResults.begin()->endpoint();

  theAcceptor.open(myEndpoint.protocol());
  theAcceptor.set_option(
      boost::asio::ip::tcp::acceptor::reuse_address(true));
  theAcceptor.bind(myEndpoint);
  theAcceptor.listen();

  LOG(INFO) << "RESP server listening on " << aEndpoint;

  theAcceptThread = std::thread([this]() { acceptLoop(); });
}

RespServer::~RespServer() {
  shutdown();
}

void RespServer::shutdown() {
  if (!theRunning.exchange(false)) {
    return;
  }

  // Wake the accept thread by self-connecting before closing the acceptor:
  // on Linux, closing the listening fd from another thread does not reliably
  // unblock a thread blocked in accept().
  //
  // The acceptor's local endpoint can be a wildcard (0.0.0.0 or ::), to
  // which connect()'s behaviour is implementation-defined and not
  // guaranteed to succeed. Always use the loopback address of the same
  // family on the local-endpoint port, and fall back to closing the
  // acceptor anyway if the connect fails — at worst the join blocks
  // until the next real client connects.
  boost::system::error_code myEc;
  if (theAcceptor.is_open()) {
    auto myLocal = theAcceptor.local_endpoint(myEc);
    if (!myEc) {
      const auto myLoopback =
          myLocal.address().is_v6()
              ? boost::asio::ip::tcp::endpoint(
                    boost::asio::ip::address_v6::loopback(), myLocal.port())
              : boost::asio::ip::tcp::endpoint(
                    boost::asio::ip::address_v4::loopback(), myLocal.port());

      boost::asio::io_context      myIo;
      boost::asio::ip::tcp::socket myWaker(myIo);
      // Async connect with a hard timeout so a firewall / dropped SYN
      // can't hang the destructor. 1s is plenty on loopback; a real
      // failure should manifest in microseconds.
      myWaker.async_connect(myLoopback,
                            [](const boost::system::error_code&) {});
      myIo.run_for(std::chrono::seconds(1));
      myWaker.close(myEc);
    }
    theAcceptor.close(myEc);
  }

  if (theAcceptThread.joinable()) {
    theAcceptThread.join();
  }

  // Close every active client socket (forces any blocked read to
  // return an error so the worker exits its loop), then join each
  // worker thread. After this point no worker can dereference theStorage,
  // so the destructor of the owning object is safe.
  std::list<Connection> myToJoin;
  {
    std::lock_guard<std::mutex> myLock(theConnectionsMutex);
    for (auto& myConn : theConnections) {
      boost::system::error_code myCloseEc;
      myConn.socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both,
                               myCloseEc);
      myConn.socket->close(myCloseEc);
    }
    // Move out so we can join without holding the mutex.
    myToJoin = std::move(theConnections);
    theConnections.clear();
  }
  for (auto& myConn : myToJoin) {
    if (myConn.worker.joinable()) {
      myConn.worker.join();
    }
  }

  LOG(INFO) << "RESP server stopped.";
}

void RespServer::acceptLoop() {
  while (theRunning) {
    try {
      boost::asio::ip::tcp::socket mySocket(theIoContext);
      theAcceptor.accept(mySocket);

      if (!theRunning) {
        break;
      }

      LOG(INFO) << "RESP client connected: "
                << mySocket.remote_endpoint();

      boost::system::error_code myNoDelayEc;
      mySocket.set_option(boost::asio::ip::tcp::no_delay(true), myNoDelayEc);

      // Track the connection so shutdown() can close its socket and
      // join its worker. The shared_ptr<socket> lets the worker thread
      // hold a stable reference while shutdown() also keeps one to
      // call close() on it from outside.
      auto mySocketPtr = std::make_shared<boost::asio::ip::tcp::socket>(
          std::move(mySocket));

      std::lock_guard<std::mutex> myLock(theConnectionsMutex);
      pruneFinishedConnectionsLocked();
      theConnections.emplace_back();
      auto& myConn  = theConnections.back();
      myConn.socket = mySocketPtr;
      myConn.worker = std::thread([this, mySocketPtr]() mutable {
        handleConnection(std::move(mySocketPtr));
      });

    } catch (const boost::system::system_error& e) {
      if (theRunning) {
        LOG(ERROR) << "RESP accept error: " << e.what();
      }
    }
  }
}

void RespServer::pruneFinishedConnectionsLocked() {
  // Caller holds theConnectionsMutex. We can't tell if a worker has
  // finished without joining it; use the heuristic that a still-open
  // socket implies a still-running worker (handleConnection only
  // returns once the socket has hit EOF / error). Prune entries whose
  // socket has been closed by the worker itself on disconnect.
  for (auto myIt = theConnections.begin(); myIt != theConnections.end();) {
    if (!myIt->socket->is_open()) {
      if (myIt->worker.joinable()) {
        myIt->worker.join();
      }
      myIt = theConnections.erase(myIt);
    } else {
      ++myIt;
    }
  }
}

void RespServer::handleConnection(
    std::shared_ptr<boost::asio::ip::tcp::socket> aSocketPtr) {
  // Top-level catch-all: any exception that escapes would propagate
  // to the thread root and terminate the process. RespHandler::handle()
  // already wraps command logic in its own try/catch and returns an
  // ERR reply on std::exception, but allocations (std::string growth
  // in myBatchedReply, RespParser frame parsing) and unforeseen
  // boost::system errors can still throw. Log and let the worker exit
  // cleanly; the shared socket is closed on the way out so the
  // accept loop can prune our entry from theConnections.
  auto& aSocket = *aSocketPtr;
  try {
    RespHandler            myHandler(theStorage);
    boost::asio::streambuf myBuffer;
    std::string            myBatchedReply;
    myBatchedReply.reserve(1024);

    auto myProcess = [&](const std::vector<std::string>& aCmd) {
      myBatchedReply.append(myHandler.handle(aCmd));
      return toUpper(aCmd[0]) == "QUIT";
    };

    while (theRunning) {
      // Block for at least one command.
      auto myCmd = RespParser::readCommand(aSocket, myBuffer);
      if (!myCmd || myCmd->empty()) {
        break;
      }

      bool myQuit = myProcess(*myCmd);

      // Drain any further already-buffered pipelined commands so we can
      // coalesce their responses into a single socket write. With Nagle
      // off (set in acceptLoop) plus batched writes, -P N benchmarks
      // scale properly instead of stalling one reply per delayed-ACK.
      while (!myQuit && theRunning && myBuffer.size() > 0) {
        auto myMore = RespParser::readCommand(aSocket, myBuffer);
        if (!myMore || myMore->empty()) {
          myQuit = true;
          break;
        }
        myQuit = myProcess(*myMore);
      }

      try {
        boost::asio::write(aSocket, boost::asio::buffer(myBatchedReply));
      } catch (const boost::system::system_error&) {
        break;
      }
      myBatchedReply.clear();

      if (myQuit) {
        break;
      }
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "RESP connection handler threw: " << e.what();
  } catch (...) {
    LOG(ERROR) << "RESP connection handler threw: unknown exception";
  }

  // Close the socket so the next pruneFinishedConnectionsLocked() can
  // remove this connection from theConnections.
  boost::system::error_code myEc;
  aSocket.close(myEc);

  LOG(INFO) << "RESP client disconnected.";
}

} // namespace okts::resp
