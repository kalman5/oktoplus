#include "Resp/resp_handler.h"
#include "Resp/resp_parser.h"
#include "Storage/storage_context.h"

#include "gtest/gtest.h"

namespace okresp = okts::resp;
namespace okstor = okts::stor;

class TestRespHandler : public ::testing::Test
{
 public:
  okstor::StorageContext theStorage;
  okresp::RespHandler    theHandler{theStorage};
};

TEST_F(TestRespHandler, ping_default) {
  EXPECT_EQ("+PONG\r\n", theHandler.handle({"PING"}));
}

TEST_F(TestRespHandler, ping_with_message_echoes) {
  EXPECT_EQ("$5\r\nhello\r\n", theHandler.handle({"PING", "hello"}));
}

TEST_F(TestRespHandler, ping_lowercase_is_dispatched) {
  EXPECT_EQ("+PONG\r\n", theHandler.handle({"ping"}));
}

TEST_F(TestRespHandler, empty_command_is_error) {
  EXPECT_EQ("-ERR empty command\r\n", theHandler.handle({}));
}

TEST_F(TestRespHandler, unknown_command_is_error) {
  auto myReply = theHandler.handle({"NOPE"});
  EXPECT_EQ("-ERR unknown command 'NOPE'\r\n", myReply);
}

TEST_F(TestRespHandler, lpush_returns_list_size) {
  EXPECT_EQ(":3\r\n", theHandler.handle({"LPUSH", "K", "a", "b", "c"}));
  EXPECT_EQ(":5\r\n", theHandler.handle({"LPUSH", "K", "d", "e"}));
}

TEST_F(TestRespHandler, lpush_missing_args_is_error) {
  auto myReply = theHandler.handle({"LPUSH", "K"});
  EXPECT_EQ("-ERR wrong number of arguments for 'lpush' command\r\n", myReply);
}

TEST_F(TestRespHandler, rpush_then_lpop_single_returns_bulk_string) {
  ASSERT_EQ(":3\r\n", theHandler.handle({"RPUSH", "K", "a", "b", "c"}));
  EXPECT_EQ("$1\r\na\r\n", theHandler.handle({"LPOP", "K"}));
}

TEST_F(TestRespHandler, lpop_with_count_returns_array) {
  ASSERT_EQ(":3\r\n", theHandler.handle({"RPUSH", "K", "a", "b", "c"}));
  EXPECT_EQ("*2\r\n$1\r\na\r\n$1\r\nb\r\n",
            theHandler.handle({"LPOP", "K", "2"}));
}

TEST_F(TestRespHandler, lpop_empty_key_returns_null) {
  EXPECT_EQ("$-1\r\n", theHandler.handle({"LPOP", "missing"}));
}

TEST_F(TestRespHandler, rpop_returns_last_element) {
  ASSERT_EQ(":3\r\n", theHandler.handle({"RPUSH", "K", "a", "b", "c"}));
  EXPECT_EQ("$1\r\nc\r\n", theHandler.handle({"RPOP", "K"}));
}

TEST_F(TestRespHandler, llen_returns_size) {
  ASSERT_EQ(":2\r\n", theHandler.handle({"RPUSH", "K", "a", "b"}));
  EXPECT_EQ(":2\r\n", theHandler.handle({"LLEN", "K"}));
}

TEST_F(TestRespHandler, sadd_then_sintercard_intersects) {
  ASSERT_EQ(":3\r\n", theHandler.handle({"SADD", "A", "x", "y", "z"}));
  ASSERT_EQ(":2\r\n", theHandler.handle({"SADD", "B", "y", "z"}));
  EXPECT_EQ(":2\r\n", theHandler.handle({"SINTERCARD", "2", "A", "B"}));
}

TEST_F(TestRespHandler, sismember_present_and_absent) {
  ASSERT_EQ(":2\r\n", theHandler.handle({"SADD", "S", "x", "y"}));
  EXPECT_EQ(":1\r\n", theHandler.handle({"SISMEMBER", "S", "x"}));
  EXPECT_EQ(":0\r\n", theHandler.handle({"SISMEMBER", "S", "z"}));
}
