#include "Configurations/jsonconfiguration.h"

#include <jsoncpp/json/json.h>

#include <fstream>

namespace okts {
namespace cfg {

JsonConfiguration::JsonConfiguration(
    const std::string& aPathConfigurationFile) {
  std::ifstream myConfigurationStream(aPathConfigurationFile,
                                      std::ifstream::binary);

  if (!myConfigurationStream) {
    throw std::runtime_error("not able to open configuration file");
  }

  Json::Value myRoot;

  myConfigurationStream >> myRoot;

  if (!myRoot) {
    throw std::runtime_error("invalid json parsing");
  }

  if (myRoot.isMember("endpoint")) {
    theEndpoint = myRoot["endpoint"].asString();
  } else {
    throw std::runtime_error("endpoint not found");
  }
}

void JsonConfiguration::dump() {
}

} // namespace cfg
} // namespace okts
