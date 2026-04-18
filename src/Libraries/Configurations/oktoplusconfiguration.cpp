#include "Configurations/oktoplusconfiguration.h"

#include <fstream>

namespace okts::cfgs {

OktoplusConfiguration::OktoplusConfiguration()
    : theEndpoint("127.0.0.1:6666")
    , theNumCQS(10)
    , theMinPollers(10)
    , theMaxPollers(20)
    , theRespEndpoint() {
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

bool OktoplusConfiguration::hasRespEndpoint() const {
  return !theRespEndpoint.empty();
}

const std::string& OktoplusConfiguration::respEndpoint() const {
  return theRespEndpoint;
}

} // namespace okts::cfgs
