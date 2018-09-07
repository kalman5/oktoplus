#pragma once

#include "Support/noncopyable.h"

#include <string>

namespace okts {
namespace cfgs {

class OktoplusConfiguration
{
 public:
  DISABLE_EVIL_CONSTRUCTOR(OktoplusConfiguration);

  OktoplusConfiguration();

  virtual ~OktoplusConfiguration() = default;

  void addConfiguration(const OktoplusConfiguration& aConfiguration);

  const std::string& endpoint() const;

 protected:
  std::string theEndpoint;
};

} // namespace cfg
} // namespace okts
