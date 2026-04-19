#include "Resp/resp_server.h"
#include "Resp/resp_handler.h"
#include "Resp/resp_parser.h"

#include <glog/logging.h>

#include <algorithm>
#include <cctype>

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
  boost::system::error_code myEc;
  if (theAcceptor.is_open()) {
    auto myLocal = theAcceptor.local_endpoint(myEc);
    if (!myEc) {
      boost::asio::io_context       myIo;
      boost::asio::ip::tcp::socket  myWaker(myIo);
      myWaker.connect(myLocal, myEc);
      myWaker.close(myEc);
    }
    theAcceptor.close(myEc);
  }

  if (theAcceptThread.joinable()) {
    theAcceptThread.join();
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

      std::thread([this, s = std::move(mySocket)]() mutable {
        handleConnection(std::move(s));
      }).detach();

    } catch (const boost::system::system_error& e) {
      if (theRunning) {
        LOG(ERROR) << "RESP accept error: " << e.what();
      }
    }
  }
}

void RespServer::handleConnection(
    boost::asio::ip::tcp::socket aSocket) {
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
    // coalesce their responses into a single socket write. With Nagle off
    // (set in acceptLoop) plus batched writes, -P N benchmarks scale
    // properly instead of stalling one reply per delayed-ACK.
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

  LOG(INFO) << "RESP client disconnected.";
}

} // namespace okts::resp
