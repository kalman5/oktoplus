#pragma once

#include "Support/noncopyable.h"

#include <string>

namespace okts::cfgs {

class OktoplusConfiguration
{
 public:
  OktoplusConfiguration();
  OktoplusConfiguration(const OktoplusConfiguration&) = default;

  virtual ~OktoplusConfiguration() = default;

  const std::string& endpoint() const;
  int                numCqs() const;
  int                minPollers() const;
  int                maxPollers() const;

  bool               hasRespEndpoint() const;
  const std::string& respEndpoint() const;

  // gRPC is opt-in (empty endpoint = disabled). RESP is always-on with
  // a default of 127.0.0.1:6379 unless the JSON overrides it.
  bool               hasGrpcEndpoint() const;

 protected:
  std::string theEndpoint;
  int         theNumCQS;
  int         theMinPollers;
  int         theMaxPollers;
  std::string theRespEndpoint;
};

} // namespace okts::cfgs
