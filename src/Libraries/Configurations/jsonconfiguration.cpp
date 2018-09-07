#include "Configurations/jsonconfiguration.h"

#include "Configurations/oktoplusconfiguration.h"

#include <jsoncpp/json/json.h>

#include <fstream>
#include <sstream>

namespace okts {
namespace cfgs {

JsonConfiguration::JsonConfiguration(
    const std::string& aConfigurationFile)
    : OktoplusConfiguration()
    , theConfigurationFile(aConfigurationFile)
{
  std::ifstream myConfigurationStream(aConfigurationFile,
                                      std::ifstream::binary);

  if (!myConfigurationStream) {
    std::stringstream myError;
    myError << "Not able to open configuration file: '"
            << aConfigurationFile << "'";
    throw std::runtime_error(myError.str());
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

JsonConfiguration::JsonConfiguration(const OktoplusConfiguration& aConfiguration)
    : OktoplusConfiguration()
    , theConfigurationFile()
{
  theEndpoint = aConfiguration.endpoint();
}

void JsonConfiguration::dump() {
  if (theConfigurationFile.empty()) {
    throw std::runtime_error("Can not json dump without a file");
  }


}

void JsonConfiguration::dump(const std::string& aConfigurationFile) {

  std::ofstream myConfigurationStream(aConfigurationFile,
                                      std::ofstream::binary);

  if (!myConfigurationStream) {
    throw std::runtime_error("not able to open configuration file");
  }

  Json::Value myRoot;

  myRoot["endpoint"] = theEndpoint;

  myConfigurationStream << myRoot;

}

} // namespace cfg
} // namespace okts
