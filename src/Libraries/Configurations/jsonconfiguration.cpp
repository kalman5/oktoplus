#include "Configurations/jsonconfiguration.h"

#include "Configurations/oktoplusconfiguration.h"

#include <json/json.h>

#include <fstream>
#include <sstream>

namespace okts::cfgs {

JsonConfiguration::JsonConfiguration(const std::string& aConfigurationFile)
    : OktoplusConfiguration()
    , theConfigurationFile(aConfigurationFile) {
  std::ifstream myConfigurationStream(aConfigurationFile,
                                      std::ifstream::binary);

  if (!myConfigurationStream) {
    std::stringstream myError;
    myError << "Not able to open configuration file: '" << aConfigurationFile
            << "'";
    throw std::runtime_error(myError.str());
  }

  Json::Value myRoot;

  myConfigurationStream >> myRoot;

  if (!myRoot) {
    throw std::runtime_error("invalid json parsing");
  }

  if (!myRoot.isMember("service")) {
    throw std::runtime_error("service section not found.");
  }

  auto myServiceConfiguration = myRoot["service"];

  // gRPC is opt-in: omitting `endpoint` disables it. The poller knobs
  // are only meaningful when gRPC is enabled, but they're allowed
  // either way (we just keep the defaults if they're absent).
  if (myServiceConfiguration.isMember("endpoint")) {
    theEndpoint = myServiceConfiguration["endpoint"].asString();
  }

  if (myServiceConfiguration.isMember("numcqs")) {
    theNumCQS = myServiceConfiguration["numcqs"].asInt();
  }

  if (myServiceConfiguration.isMember("minpollers")) {
    theMinPollers = myServiceConfiguration["minpollers"].asInt();
  }

  if (myServiceConfiguration.isMember("maxpollers")) {
    theMaxPollers = myServiceConfiguration["maxpollers"].asInt();
  }

  // RESP is always-on. Absence of `resp_endpoint` keeps the built-in
  // default (127.0.0.1:6379); presence overrides it.
  if (myServiceConfiguration.isMember("resp_endpoint")) {
    theRespEndpoint = myServiceConfiguration["resp_endpoint"].asString();
  }
}

JsonConfiguration::JsonConfiguration(
    const OktoplusConfiguration& aConfiguration)
    : OktoplusConfiguration(aConfiguration)
    , theConfigurationFile() {
}

void JsonConfiguration::dump() {
  if (theConfigurationFile.empty()) {
    throw std::runtime_error("Can not json dump without a file");
  }
  dump(theConfigurationFile);
}

void JsonConfiguration::dump(const std::string& aConfigurationFile) {

  std::ofstream myConfigurationStream(aConfigurationFile,
                                      std::ofstream::binary);

  if (!myConfigurationStream) {
    throw std::runtime_error("not able to open configuration file");
  }

  Json::Value myRoot;

  // gRPC fields only round-trip when gRPC is enabled. Writing
  // `endpoint: ""` would round-trip to "still disabled" on load but
  // bloats the file with unused poller knobs, so we skip the whole
  // block when gRPC is off.
  if (!theEndpoint.empty()) {
    myRoot["service"]["endpoint"]   = theEndpoint;
    myRoot["service"]["numcqs"]     = theNumCQS;
    myRoot["service"]["minpollers"] = theMinPollers;
    myRoot["service"]["maxpollers"] = theMaxPollers;
  }

  if (!theRespEndpoint.empty()) {
    myRoot["service"]["resp_endpoint"] = theRespEndpoint;
  }

  myConfigurationStream << myRoot;
}

} // namespace okts::cfgs
