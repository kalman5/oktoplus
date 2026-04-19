#include "Resp/resp_server.h"
#include "Storage/storage_context.h"

#include "gtest/gtest.h"

#include <boost/asio.hpp>

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

namespace okresp = okts::resp;
namespace okstor = okts::stor;
namespace asio   = boost::asio;

namespace {

class RespClient
{
 public:
  RespClient(const std::string& aHost, uint16_t aPort)
      : theIo()
      , theSocket(theIo) {
    asio::ip::tcp::resolver myResolver(theIo);
    asio::connect(theSocket, myResolver.resolve(aHost, std::to_string(aPort)));
  }

  void send(const std::string& aPayload) {
    asio::write(theSocket, asio::buffer(aPayload));
  }

  std::string readUntilCrlf() {
    asio::read_until(theSocket, theBuffer, "\r\n");
    std::istream myStream(&theBuffer);
    std::string  myLine;
    std::getline(myStream, myLine);
    if (!myLine.empty() && myLine.back() == '\r') {
      myLine.pop_back();
    }
    return myLine;
  }

  std::string readBytes(size_t aN) {
    if (theBuffer.size() < aN) {
      asio::read(
          theSocket, theBuffer, asio::transfer_at_least(aN - theBuffer.size()));
    }
    std::string myData(aN, '\0');
    std::istream myStream(&theBuffer);
    myStream.read(myData.data(), aN);
    return myData;
  }

  // Sends args as a RESP array of bulk strings and returns the first response
  // line (e.g. "+PONG", ":3", "$5", "*2", "-ERR ...").
  std::string command(const std::vector<std::string>& aArgs) {
    std::string myReq = "*" + std::to_string(aArgs.size()) + "\r\n";
    for (const auto& myArg : aArgs) {
      myReq += "$" + std::to_string(myArg.size()) + "\r\n" + myArg + "\r\n";
    }
    send(myReq);
    return readUntilCrlf();
  }

  void close() {
    boost::system::error_code myEc;
    theSocket.shutdown(asio::ip::tcp::socket::shutdown_both, myEc);
    theSocket.close(myEc);
  }

 private:
  asio::io_context       theIo;
  asio::ip::tcp::socket  theSocket;
  asio::streambuf        theBuffer;
};

// Ask the OS for an unused port by binding a temporary acceptor to port 0,
// reading back the assigned port, then releasing it.
uint16_t pickFreePort() {
  asio::io_context        myIo;
  asio::ip::tcp::acceptor myAcc(
      myIo,
      asio::ip::tcp::endpoint(asio::ip::make_address_v4("127.0.0.1"), 0));
  return myAcc.local_endpoint().port();
}

} // namespace

class TestRespServer : public ::testing::Test
{
 public:
  TestRespServer()
      : thePort(pickFreePort())
      , theEndpoint("127.0.0.1:" + std::to_string(thePort))
      , theStorage()
      , theServer(theStorage, theEndpoint) {
    // Give the accept thread a moment to be ready.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  uint16_t                thePort;
  std::string             theEndpoint;
  okstor::StorageContext  theStorage;
  okresp::RespServer      theServer;
};

TEST_F(TestRespServer, invalid_endpoint_throws) {
  okstor::StorageContext myStorage;
  EXPECT_THROW(okresp::RespServer(myStorage, "no-colon-here"),
               std::runtime_error);
}

TEST_F(TestRespServer, ping_round_trip) {
  RespClient myClient("127.0.0.1", thePort);
  EXPECT_EQ("+PONG", myClient.command({"PING"}));
}

TEST_F(TestRespServer, unknown_command_returns_error) {
  RespClient myClient("127.0.0.1", thePort);
  auto myReply = myClient.command({"NOPE"});
  EXPECT_EQ("-ERR unknown command 'NOPE'", myReply);
}

TEST_F(TestRespServer, lpush_lpop_round_trip) {
  RespClient myClient("127.0.0.1", thePort);

  EXPECT_EQ(":3", myClient.command({"RPUSH", "K", "a", "b", "c"}));

  // LPOP K -> bulk string "a"
  EXPECT_EQ("$1", myClient.command({"LPOP", "K"}));
  // payload + trailing \r\n
  EXPECT_EQ("a", myClient.readBytes(1));
  EXPECT_EQ("", myClient.readUntilCrlf());
}

TEST_F(TestRespServer, two_clients_share_storage) {
  RespClient myWriter("127.0.0.1", thePort);
  EXPECT_EQ(":2", myWriter.command({"RPUSH", "shared", "x", "y"}));

  RespClient myReader("127.0.0.1", thePort);
  EXPECT_EQ(":2", myReader.command({"LLEN", "shared"}));
}

TEST_F(TestRespServer, quit_closes_connection) {
  RespClient myClient("127.0.0.1", thePort);
  EXPECT_EQ("+OK", myClient.command({"QUIT"}));

  // Server must have closed: a subsequent read should fail (EOF).
  EXPECT_THROW(myClient.readUntilCrlf(), boost::system::system_error);
}

TEST_F(TestRespServer, shutdown_is_idempotent) {
  theServer.shutdown();
  EXPECT_NO_THROW(theServer.shutdown());
}
