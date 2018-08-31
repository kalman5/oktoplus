#include "Commands/commands_client.h"
#include "Commands/commands_server.h"

#include "gtest/gtest.h"

#include <glog/logging.h>

namespace okco = okts::commands;

class TestCommands : public ::testing::Test
{
 public:
  TestCommands() {
  }

  void SetUp() override {
  }
};

TEST_F(TestCommands, server_not_available) {

  okco::CommandsClient myClient("127.0.0.1:6666");
  ASSERT_THROW(myClient.listPushFront("List-1", {"value1", "value2"}),
               std::runtime_error);
}

TEST_F(TestCommands, set_get) {
  okco::CommandsServer myServer("127.0.0.1:6666");

  okco::CommandsClient myClient("127.0.0.1:6666");

  const std::size_t myListSize = 100;

  for (size_t i = 0; i < myListSize; ++i) {
    myClient.listPushFront("List-1", {std::to_string(i)});
  }

  for (size_t i = 0; i < myListSize; ++i) {
    ASSERT_EQ(std::to_string(myListSize - 1 - i),
              myClient.listPopFront("List-1"));
  }
}

TEST_F(TestCommands, DISABLED_perf) {
  okco::CommandsClient myClient("127.0.0.1:6666");

  const std::size_t myListSize = 100000;

  for (size_t i = 0; i < myListSize; ++i) {
    myClient.listPushFront("List-1",
                           {"test",
                            "test",
                            "test",
                            "test",
                            "test",
                            "test",
                            "test",
                            "test",
                            "test",
                            "test"});
  }
}

TEST_F(TestCommands, DISABLED_perf2) {
  okco::CommandsClient myClient("127.0.0.1:6666");

  const std::size_t myListSize = 100000;

  for (size_t i = 0; i < myListSize; ++i) {
    myClient.listPushFront("List-2", {"test"});
  }
}
