#include "Resp/resp_parser.h"

#include <charconv>
#include <cstring>
#include <string_view>

namespace okts::resp {

namespace {

// View of the streambuf input area as a contiguous span. boost::asio
// streambuf is a single-buffer implementation so data() yields one
// contiguous range.
std::string_view bufView(const boost::asio::streambuf& aBuffer) {
  auto myBuf = aBuffer.data();
  return std::string_view(boost::asio::buffer_cast<const char*>(myBuf),
                          boost::asio::buffer_size(myBuf));
}

// Block until aBuffer contains \r\n, then return everything up to (not
// including) the delimiter and consume line + delimiter from aBuffer.
// Avoids std::istream / std::getline (slow due to locale/sentry).
std::string readLineFromBuffer(boost::asio::ip::tcp::socket& aSocket,
                               boost::asio::streambuf&       aBuffer) {
  boost::asio::read_until(aSocket, aBuffer, "\r\n");
  auto myView = bufView(aBuffer);
  auto myEnd  = myView.find("\r\n");
  // read_until guarantees presence; treat absence as a parse error.
  if (myEnd == std::string_view::npos) {
    return {};
  }
  std::string myLine(myView.data(), myEnd);
  aBuffer.consume(myEnd + 2);
  return myLine;
}

// Parse a non-negative or signed integer in [aPtr, aPtr+aLen) into aOut.
// Returns true on success, false on any junk. No exceptions, no locale.
template <class T>
bool parseInt(const char* aPtr, size_t aLen, T& aOut) {
  if (aLen == 0) return false;
  auto myRes = std::from_chars(aPtr, aPtr + aLen, aOut);
  return myRes.ec == std::errc{} && myRes.ptr == aPtr + aLen;
}

} // namespace

std::string RespParser::readLine(boost::asio::ip::tcp::socket& aSocket,
                                 boost::asio::streambuf&       aBuffer) {
  return readLineFromBuffer(aSocket, aBuffer);
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
  auto        myView = bufView(aBuffer);
  std::string myData(myView.data(), static_cast<size_t>(aLength));
  aBuffer.consume(myNeeded);
  return myData;
}

std::optional<std::vector<std::string>>
RespParser::readCommand(boost::asio::ip::tcp::socket& aSocket,
                        boost::asio::streambuf&       aBuffer) {
  try {
    auto myLine = readLineFromBuffer(aSocket, aBuffer);
    if (myLine.empty()) {
      return std::nullopt;
    }

    // RESP array: *N\r\n
    if (myLine[0] == '*') {
      int64_t myCount = 0;
      if (!parseInt(myLine.data() + 1, myLine.size() - 1, myCount) ||
          myCount < 0) {
        return std::nullopt;
      }

      std::vector<std::string> myArgs;
      myArgs.reserve(static_cast<size_t>(myCount));

      for (int64_t i = 0; i < myCount; ++i) {
        auto myArgLine = readLineFromBuffer(aSocket, aBuffer);
        if (myArgLine.empty() || myArgLine[0] != '$') {
          return std::nullopt;
        }
        int64_t myLen = 0;
        if (!parseInt(
                myArgLine.data() + 1, myArgLine.size() - 1, myLen)) {
          return std::nullopt;
        }
        if (myLen < 0) {
          myArgs.emplace_back();
        } else {
          myArgs.push_back(readBulkString(aSocket, aBuffer, myLen));
        }
      }
      return myArgs;
    }

    // Inline command: split on spaces without std::istringstream.
    std::vector<std::string> myArgs;
    std::string_view         myView(myLine);
    size_t                   myPos = 0;
    while (myPos < myView.size()) {
      while (myPos < myView.size() && myView[myPos] == ' ') ++myPos;
      const size_t myStart = myPos;
      while (myPos < myView.size() && myView[myPos] != ' ') ++myPos;
      if (myPos > myStart) {
        myArgs.emplace_back(myView.substr(myStart, myPos - myStart));
      }
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
  std::string myResult;
  myResult.reserve(8 + aValue.size());
  appendBulkString(myResult, aValue);
  return myResult;
}

std::string RespParser::formatNullBulkString() {
  return "$-1\r\n";
}

std::string
RespParser::formatArray(const std::vector<std::string>& aElements) {
  size_t myReserved = 16;
  for (const auto& myElement : aElements) {
    myReserved += myElement.size();
  }
  std::string myResult;
  myResult.reserve(myReserved);
  appendArrayHeader(myResult, aElements.size());
  for (const auto& myElement : aElements) {
    myResult += myElement;
  }
  return myResult;
}

std::string RespParser::formatEmptyArray() {
  return "*0\r\n";
}

void RespParser::appendBulkString(std::string&     aOut,
                                  std::string_view aValue) {
  aOut += '$';
  aOut += std::to_string(aValue.size());
  aOut += "\r\n";
  aOut.append(aValue.data(), aValue.size());
  aOut += "\r\n";
}

void RespParser::appendArrayHeader(std::string& aOut, size_t aCount) {
  aOut += '*';
  aOut += std::to_string(aCount);
  aOut += "\r\n";
}

void RespParser::appendInteger(std::string& aOut, int64_t aValue) {
  aOut += ':';
  aOut += std::to_string(aValue);
  aOut += "\r\n";
}

} // namespace okts::resp
