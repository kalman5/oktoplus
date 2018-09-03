#include "Configurations/defaultconfiguration.h"

#include <fstream>

namespace okts {
namespace cfg {

DefaultConfiguration::DefaultConfiguration()
    : MainConfiguration() {
  theEndpoint = "127.0.0.1:2323";
}

void DefaultConfiguration::dump() {
}

} // namespace cfg
} // namespace okts
