#include "Configurations/oktoplusconfiguration.h"

#include <fstream>

namespace okts::cfgs {

OktoplusConfiguration::OktoplusConfiguration()
    // gRPC is now opt-in: empty endpoint means "do not bind a gRPC
    // server". Set `service.endpoint` in the JSON config to enable it.
    : theEndpoint()
    , theNumCQS(10)
    , theMinPollers(10)
    , theMaxPollers(20)
    // RESP is the default wire protocol. Always-on at 6379 unless the
    // user overrides `service.resp_endpoint` in the JSON config.
    , theRespEndpoint("127.0.0.1:6379") {
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

bool OktoplusConfiguration::hasGrpcEndpoint() const {
  return !theEndpoint.empty();
}

const std::string& OktoplusConfiguration::respEndpoint() const {
  return theRespEndpoint;
}

} // namespace okts::cfgs
