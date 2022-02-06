#include "Configurations/defaultconfiguration.h"

#include <fstream>

namespace okts {
namespace cfgs {

DefaultConfiguration::DefaultConfiguration()
    : OktoplusConfiguration() {
  theEndpoint = "127.0.0.1:6666";
}

} // namespace cfgs
} // namespace okts
