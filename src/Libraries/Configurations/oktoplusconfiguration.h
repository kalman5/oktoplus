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

 protected:
  std::string theEndpoint;
  int         theNumCQS;
  int         theMinPollers;
  int         theMaxPollers;
};

} // namespace okts::cfgs
