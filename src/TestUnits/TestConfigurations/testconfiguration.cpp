#include "Configurations/defaultconfiguration.h"
#include "Configurations/jsonconfiguration.h"
#include "Configurations/oktoplusconfiguration.h"

#include "gtest/gtest.h"

#include <glog/logging.h>

namespace okcfgs = okts::cfgs;

class TestConfiguration : public ::testing::Test
{
 public:
  TestConfiguration() {
  }

  void SetUp() override {
  }
};

TEST_F(TestConfiguration, ctor) {
  okcfgs::DefaultConfiguration myDefault;

  okcfgs::OktoplusConfiguration myOktoplusConfiguration;
  myOktoplusConfiguration.addConfiguration(myDefault);
  ASSERT_EQ(myDefault.endpoint(), myOktoplusConfiguration.endpoint());

  {
    okcfgs::JsonConfiguration myJsonConfiguration(myOktoplusConfiguration);
    myJsonConfiguration.dump("oktoplus.json");
  }

  okcfgs::JsonConfiguration myJsonConfiguration("oktoplus.json");
  ASSERT_EQ(myDefault.endpoint(), myJsonConfiguration.endpoint());

  {
    okcfgs::OktoplusConfiguration myOktoplusConfiguration;
    myOktoplusConfiguration.addConfiguration(myJsonConfiguration);
    ASSERT_EQ(myDefault.endpoint(), myOktoplusConfiguration.endpoint());
  }
}
