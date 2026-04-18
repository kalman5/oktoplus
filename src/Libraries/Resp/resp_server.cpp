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

  boost::system::error_code myEc;
  theAcceptor.close(myEc);

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
  RespHandler         myHandler(theStorage);
  boost::asio::streambuf myBuffer;

  while (theRunning) {
    auto myCmd = RespParser::readCommand(aSocket, myBuffer);
    if (!myCmd || myCmd->empty()) {
      break;
    }

    auto myResponse = myHandler.handle(*myCmd);

    try {
      boost::asio::write(aSocket, boost::asio::buffer(myResponse));
    } catch (const boost::system::system_error&) {
      break;
    }

    if (toUpper((*myCmd)[0]) == "QUIT") {
      break;
    }
  }

  LOG(INFO) << "RESP client disconnected.";
}

} // namespace okts::resp
