#include "Configurations/oktoplusconfiguration.h"

#include <fstream>

namespace okts {
namespace cfgs {

OktoplusConfiguration::OktoplusConfiguration() {

}

void OktoplusConfiguration::addConfiguration(const OktoplusConfiguration& aConfiguration) {
  theEndpoint = aConfiguration.endpoint();
}

const std::string& OktoplusConfiguration::endpoint() const {
  return theEndpoint;
}

} // namespace cfg
} // namespace okts
