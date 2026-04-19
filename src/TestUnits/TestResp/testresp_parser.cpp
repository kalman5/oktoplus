#include "Resp/resp_parser.h"

#include "gtest/gtest.h"

namespace okresp = okts::resp;

class TestRespParser : public ::testing::Test
{};

TEST_F(TestRespParser, format_simple_string) {
  EXPECT_EQ("+OK\r\n", okresp::RespParser::formatSimpleString("OK"));
  EXPECT_EQ("+\r\n", okresp::RespParser::formatSimpleString(""));
}

TEST_F(TestRespParser, format_error) {
  EXPECT_EQ("-ERR boom\r\n", okresp::RespParser::formatError("ERR boom"));
}

TEST_F(TestRespParser, format_integer) {
  EXPECT_EQ(":0\r\n", okresp::RespParser::formatInteger(0));
  EXPECT_EQ(":42\r\n", okresp::RespParser::formatInteger(42));
  EXPECT_EQ(":-7\r\n", okresp::RespParser::formatInteger(-7));
}

TEST_F(TestRespParser, format_bulk_string) {
  EXPECT_EQ("$5\r\nhello\r\n", okresp::RespParser::formatBulkString("hello"));
  EXPECT_EQ("$0\r\n\r\n", okresp::RespParser::formatBulkString(""));
}

TEST_F(TestRespParser, format_bulk_string_with_binary) {
  std::string myBinary("a\0b", 3);
  auto myOut = okresp::RespParser::formatBulkString(myBinary);
  EXPECT_EQ(std::string("$3\r\na\0b\r\n", 9), myOut);
}

TEST_F(TestRespParser, format_null_bulk_string) {
  EXPECT_EQ("$-1\r\n", okresp::RespParser::formatNullBulkString());
}

TEST_F(TestRespParser, format_empty_array) {
  EXPECT_EQ("*0\r\n", okresp::RespParser::formatEmptyArray());
}

TEST_F(TestRespParser, format_array) {
  std::vector<std::string> myElems{
      okresp::RespParser::formatBulkString("a"),
      okresp::RespParser::formatBulkString("bc"),
  };
  EXPECT_EQ("*2\r\n$1\r\na\r\n$2\r\nbc\r\n",
            okresp::RespParser::formatArray(myElems));
}
