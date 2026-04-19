#pragma once

#include <boost/asio.hpp>

#include <optional>
#include <string>
#include <vector>

namespace okts::resp {

class RespParser
{
 public:
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

 private:
  static std::string readLine(boost::asio::ip::tcp::socket& aSocket,
                              boost::asio::streambuf&       aBuffer);

  static std::string readBulkString(boost::asio::ip::tcp::socket& aSocket,
                                    boost::asio::streambuf&       aBuffer,
                                    int64_t                       aLength);
};

} // namespace okts::resp
