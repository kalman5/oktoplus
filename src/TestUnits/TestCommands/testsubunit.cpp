#include <glog/logging.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

class OktoplusEnvironment : public ::testing::Environment
{
 public:
  void SetUp() override {
    srand(time(nullptr));
  }

  void TearDown() override {
  }
};

int main(int argc, char** argv) {
  bool myGTestProblem = false;

  try {
    ::google::InitGoogleLogging(argv[0]);
    ::google::LogToStderr();

    ::testing::InitGoogleMock(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new OktoplusEnvironment);


    srand(time(nullptr)); // many element in the test are randomly generated
    srand48(time(nullptr));

    // Gtest part
    myGTestProblem = RUN_ALL_TESTS();
  } catch (...) {
    std::cerr << "Unspecified error thrown" << std::endl;
    return -1;
  }

  return myGTestProblem;
}
