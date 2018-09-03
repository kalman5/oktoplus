#include "Configurations/mainconfiguration.h"

#include <fstream>

namespace okts {
namespace cfg {

MainConfiguration::MainConfiguration()
    : theEndpoint() {
}

const std::string& MainConfiguration::endpoint() const {
  return theEndpoint;
}

} // namespace cfg
} // namespace okts
