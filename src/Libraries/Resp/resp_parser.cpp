#include "Resp/resp_parser.h"

#include <sstream>

namespace okts::resp {

std::string RespParser::readLine(boost::asio::ip::tcp::socket& aSocket,
                                 boost::asio::streambuf&       aBuffer) {
  boost::asio::read_until(aSocket, aBuffer, "\r\n");
  std::istream myStream(&aBuffer);
  std::string  myLine;
  std::getline(myStream, myLine);
  if (!myLine.empty() && myLine.back() == '\r') {
    myLine.pop_back();
  }
  return myLine;
}

std::string RespParser::readBulkString(boost::asio::ip::tcp::socket& aSocket,
                                       boost::asio::streambuf&       aBuffer,
                                       int64_t                       aLength) {
  const size_t myNeeded = static_cast<size_t>(aLength) + 2; // data + \r\n
  if (aBuffer.size() < myNeeded) {
    boost::asio::read(
        aSocket,
        aBuffer,
        boost::asio::transfer_at_least(myNeeded - aBuffer.size()));
  }
  std::string  myData(aLength, '\0');
  std::istream myStream(&aBuffer);
  myStream.read(myData.data(), aLength);
  // consume trailing \r\n
  char myDiscard[2];
  myStream.read(myDiscard, 2);
  return myData;
}

std::optional<std::vector<std::string>>
RespParser::readCommand(boost::asio::ip::tcp::socket& aSocket,
                        boost::asio::streambuf&       aBuffer) {
  try {
    auto myLine = readLine(aSocket, aBuffer);

    if (myLine.empty()) {
      return std::nullopt;
    }

    // RESP array: *N\r\n
    if (myLine[0] == '*') {
      const int64_t myCount = std::stoll(myLine.substr(1));
      if (myCount < 0) {
        return std::nullopt;
      }

      std::vector<std::string> myArgs;
      myArgs.reserve(static_cast<size_t>(myCount));

      for (int64_t i = 0; i < myCount; ++i) {
        auto myArgLine = readLine(aSocket, aBuffer);
        if (myArgLine.empty() || myArgLine[0] != '$') {
          return std::nullopt;
        }
        const int64_t myLen = std::stoll(myArgLine.substr(1));
        if (myLen < 0) {
          myArgs.emplace_back();
        } else {
          myArgs.push_back(readBulkString(aSocket, aBuffer, myLen));
        }
      }
      return myArgs;
    }

    // Inline command: split by spaces
    std::vector<std::string> myArgs;
    std::istringstream       myIss(myLine);
    std::string              myToken;
    while (myIss >> myToken) {
      myArgs.push_back(std::move(myToken));
    }
    if (myArgs.empty()) {
      return std::nullopt;
    }
    return myArgs;

  } catch (const boost::system::system_error&) {
    return std::nullopt;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::string RespParser::formatSimpleString(std::string_view aValue) {
  std::string myResult;
  myResult.reserve(1 + aValue.size() + 2);
  myResult += '+';
  myResult.append(aValue);
  myResult += "\r\n";
  return myResult;
}

std::string RespParser::formatError(std::string_view aMessage) {
  std::string myResult;
  myResult.reserve(1 + aMessage.size() + 2);
  myResult += '-';
  myResult.append(aMessage);
  myResult += "\r\n";
  return myResult;
}

std::string RespParser::formatInteger(int64_t aValue) {
  auto myStr = std::to_string(aValue);
  std::string myResult;
  myResult.reserve(1 + myStr.size() + 2);
  myResult += ':';
  myResult += myStr;
  myResult += "\r\n";
  return myResult;
}

std::string RespParser::formatBulkString(std::string_view aValue) {
  auto myLen = std::to_string(aValue.size());
  std::string myResult;
  myResult.reserve(1 + myLen.size() + 2 + aValue.size() + 2);
  myResult += '$';
  myResult += myLen;
  myResult += "\r\n";
  myResult.append(aValue);
  myResult += "\r\n";
  return myResult;
}

std::string RespParser::formatNullBulkString() {
  return "$-1\r\n";
}

std::string
RespParser::formatArray(const std::vector<std::string>& aElements) {
  std::string myResult;
  myResult += '*';
  myResult += std::to_string(aElements.size());
  myResult += "\r\n";
  for (const auto& myElement : aElements) {
    myResult += myElement;
  }
  return myResult;
}

std::string RespParser::formatEmptyArray() {
  return "*0\r\n";
}

} // namespace okts::resp
