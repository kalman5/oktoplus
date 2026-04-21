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

// Parse a non-negative or signed integer in [aPtr, aPtr+aLen) into aOut.
// Returns true on success, false on any junk. No exceptions, no locale.
template <class T>
bool parseInt(const char* aPtr, size_t aLen, T& aOut) {
  if (aLen == 0) return false;
  auto myRes = std::from_chars(aPtr, aPtr + aLen, aOut);
  return myRes.ec == std::errc{} && myRes.ptr == aPtr + aLen;
}

// Per-element bulk-string cap (matches Redis's proto-max-bulk-len
// default of 512 MiB). Anything larger is almost certainly a malformed
// or hostile frame; honouring the wire-supplied length without a cap
// lets a remote client request an 8-EiB allocation by sending
// '$9223372036854775806\r\n'.
constexpr int64_t kMaxBulkStringLen = 512LL * 1024 * 1024;
constexpr int64_t kMaxArrayElements = 512 * 1024;

} // namespace

RespParser::ParseOutput
RespParser::tryParseCommand(boost::asio::streambuf& aBuffer) {
  ParseOutput myOut; // status defaults to NeedMore

  const auto myView = bufView(aBuffer);

  // Find the *next* "\r\n" starting at aFrom; npos if absent.
  auto findCrlf = [&](size_t aFrom) {
    return myView.find("\r\n", aFrom);
  };

  size_t myEnd = findCrlf(0);
  if (myEnd == std::string_view::npos) {
    return myOut; // NeedMore for the header line.
  }

  const std::string_view myFirstLine = myView.substr(0, myEnd);
  size_t                 myPos       = myEnd + 2;

  if (myFirstLine.empty()) {
    myOut.status = ParseStatus::Error;
    return myOut;
  }

  if (myFirstLine[0] == '*') {
    int64_t myCount = 0;
    if (!parseInt(myFirstLine.data() + 1, myFirstLine.size() - 1, myCount) ||
        myCount < 0 || myCount > kMaxArrayElements) {
      myOut.status = ParseStatus::Error;
      return myOut;
    }

    myOut.args.reserve(static_cast<size_t>(myCount));

    for (int64_t i = 0; i < myCount; ++i) {
      const size_t myArgEnd = findCrlf(myPos);
      if (myArgEnd == std::string_view::npos) {
        // Frame is incomplete — leave aBuffer untouched and let the
        // caller fill more bytes before re-trying.
        myOut.args.clear();
        return myOut;
      }
      const std::string_view myArgLine =
          myView.substr(myPos, myArgEnd - myPos);
      const size_t myArgLineEnd = myArgEnd + 2;

      if (myArgLine.empty() || myArgLine[0] != '$') {
        myOut.status = ParseStatus::Error;
        return myOut;
      }
      int64_t myLen = 0;
      if (!parseInt(myArgLine.data() + 1, myArgLine.size() - 1, myLen) ||
          myLen > kMaxBulkStringLen) {
        myOut.status = ParseStatus::Error;
        return myOut;
      }
      if (myLen < 0) {
        myOut.args.emplace_back();
        myPos = myArgLineEnd;
      } else {
        const size_t myNeeded = myArgLineEnd + static_cast<size_t>(myLen) + 2;
        if (myView.size() < myNeeded) {
          // Bulk-string body (or its trailing CRLF) not yet here.
          myOut.args.clear();
          return myOut;
        }
        myOut.args.emplace_back(myView.data() + myArgLineEnd,
                                static_cast<size_t>(myLen));
        myPos = myNeeded;
      }
    }

    aBuffer.consume(myPos);
    myOut.status = ParseStatus::Ok;
    return myOut;
  }

  // Inline command: split on spaces without std::istringstream.
  size_t myInlinePos = 0;
  while (myInlinePos < myFirstLine.size()) {
    while (myInlinePos < myFirstLine.size() &&
           myFirstLine[myInlinePos] == ' ') {
      ++myInlinePos;
    }
    const size_t myStart = myInlinePos;
    while (myInlinePos < myFirstLine.size() &&
           myFirstLine[myInlinePos] != ' ') {
      ++myInlinePos;
    }
    if (myInlinePos > myStart) {
      myOut.args.emplace_back(
          myFirstLine.substr(myStart, myInlinePos - myStart));
    }
  }
  if (myOut.args.empty()) {
    myOut.status = ParseStatus::Error;
    return myOut;
  }
  aBuffer.consume(myPos);
  myOut.status = ParseStatus::Ok;
  return myOut;
}

std::optional<std::vector<std::string>>
RespParser::readCommand(boost::asio::ip::tcp::socket& aSocket,
                        boost::asio::streambuf&       aBuffer) {
  for (;;) {
    auto myOut = tryParseCommand(aBuffer);
    if (myOut.status == ParseStatus::Ok) {
      return std::move(myOut.args);
    }
    if (myOut.status == ParseStatus::Error) {
      return std::nullopt;
    }
    // NeedMore: pull at least one more byte and re-attempt the parse.
    try {
      auto         myMutBuf = aBuffer.prepare(4096);
      const size_t myN      = aSocket.read_some(myMutBuf);
      if (myN == 0) {
        return std::nullopt;
      }
      aBuffer.commit(myN);
    } catch (const boost::system::system_error&) {
      return std::nullopt;
    }
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
