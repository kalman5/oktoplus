#pragma once

#include <boost/asio.hpp>

#include <optional>
#include <string>
#include <vector>

namespace okts::resp {

class RespParser
{
 public:
  // Outcome of a buffer-only parse attempt. Async paths use
  // tryParseCommand directly so they can return control to the
  // io_context when more bytes are needed instead of blocking.
  enum class ParseStatus { Ok, NeedMore, Error };

  struct ParseOutput {
    ParseStatus              status = ParseStatus::NeedMore;
    std::vector<std::string> args;
  };

  // Pure buffer consumer: never reads from a socket, never throws,
  // and never partially consumes the buffer. On NeedMore the buffer
  // is left untouched so the next call (after more bytes arrive) can
  // re-parse from the start of the frame.
  static ParseOutput tryParseCommand(boost::asio::streambuf& aBuffer);

  // Sync read-and-parse for callers that don't run an io_context
  // (tests, future tooling). Drives tryParseCommand in a loop and
  // fills the buffer with read_some() between attempts.
  static std::optional<std::vector<std::string>>
  readCommand(boost::asio::ip::tcp::socket& aSocket,
              boost::asio::streambuf&       aBuffer);

  static std::string formatSimpleString(std::string_view aValue);
  static std::string formatError(std::string_view aMessage);
  static std::string formatInteger(int64_t aValue);
  static std::string formatBulkString(std::string_view aValue);
  static std::string formatNullBulkString();
  static std::string formatArray(const std::vector<std::string>& aElements);
  static std::string formatEmptyArray();

  // Append-into-out variants for the hot reply path: writers like
  // formatBulkStringArray() produce N bulk strings + an array header,
  // which used to allocate N intermediate std::string's. The append
  // overloads write directly into a caller-owned buffer instead.
  static void appendBulkString(std::string& aOut, std::string_view aValue);
  static void appendArrayHeader(std::string& aOut, size_t aCount);
  static void appendInteger(std::string& aOut, int64_t aValue);

};

} // namespace okts::resp
