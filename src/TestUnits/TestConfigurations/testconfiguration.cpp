#include "Configurations/jsonconfiguration.h"
#include "Configurations/oktoplusconfiguration.h"

#include "gtest/gtest.h"

#include <jsoncpp/json/json.h>

#include <cstdio>
#include <fstream>
#include <glog/logging.h>

namespace okcfgs = okts::cfgs;

class TestConfiguration : public ::testing::Test
{
 public:
  TestConfiguration() {
  }

  void SetUp() override {
    std::remove("configuration.json");
  }

  void TearDown() override {
    std::remove("configuration.json");
  }
};

TEST_F(TestConfiguration, ctor) {
  okcfgs::OktoplusConfiguration myDefault;

  {
    okcfgs::JsonConfiguration myJsonConfiguration(myDefault);
    myJsonConfiguration.dump("oktoplus.json");
  }

  okcfgs::JsonConfiguration myJsonConfiguration("oktoplus.json");
  ASSERT_EQ(myDefault.endpoint(), myJsonConfiguration.endpoint());
  ASSERT_EQ(myDefault.numCqs(), myJsonConfiguration.numCqs());
  ASSERT_EQ(myDefault.minPollers(), myJsonConfiguration.minPollers());
  ASSERT_EQ(myDefault.maxPollers(), myJsonConfiguration.maxPollers());

  {
    okcfgs::OktoplusConfiguration myOktoplusConfiguration(myJsonConfiguration);
    ASSERT_EQ(myDefault.endpoint(), myOktoplusConfiguration.endpoint());
    ASSERT_EQ(myDefault.numCqs(), myJsonConfiguration.numCqs());
    ASSERT_EQ(myDefault.minPollers(), myJsonConfiguration.minPollers());
    ASSERT_EQ(myDefault.maxPollers(), myJsonConfiguration.maxPollers());
  }
}

TEST_F(TestConfiguration, json_parse) {
  std::string myExpectedEndpoint("10.10.10.10:8888");
  int         myExpecteNumCqs     = 30;
  int         myExpecteMinPollers = 31;
  int         myExpecteMaxPollers = 32;
  {
    std::ofstream myConfigurationStream("configuration.json",
                                        std::ofstream::binary);

    Json::Value myRoot;

    myRoot["service"]["endpoint"]   = "10.10.10.10:8888";
    myRoot["service"]["numcqs"]     = myExpecteNumCqs;
    myRoot["service"]["minpollers"] = myExpecteMinPollers;
    myRoot["service"]["maxpollers"] = myExpecteMaxPollers;

    myConfigurationStream << myRoot;
  }

  okcfgs::JsonConfiguration myJsonConfiguration("configuration.json");
  ASSERT_EQ(myExpectedEndpoint, myJsonConfiguration.endpoint());
  ASSERT_EQ(myExpecteNumCqs, myJsonConfiguration.numCqs());
  ASSERT_EQ(myExpecteMinPollers, myJsonConfiguration.minPollers());
  ASSERT_EQ(myExpecteMaxPollers, myJsonConfiguration.maxPollers());
}
