#include "Configurations/oktoplusconfiguration.h"

#include <fstream>

namespace okts {
namespace cfgs {

OktoplusConfiguration::OktoplusConfiguration()
    : theEndpoint("127.0.0.1:6666")
    , theNumCQS(20)
    , theMinPollers(10)
    , theMaxPollers(30) {
}

const std::string& OktoplusConfiguration::endpoint() const {
  return theEndpoint;
}

int OktoplusConfiguration::numCqs() const {
  return theNumCQS;
}
int OktoplusConfiguration::minPollers() const {
  return theMinPollers;
}
int OktoplusConfiguration::maxPollers() const {
  return theMaxPollers;
}

} // namespace cfgs
} // namespace okts
