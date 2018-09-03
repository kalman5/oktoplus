#pragma once

#include "Support/noncopyable.h"

#include <string>

namespace okts {
namespace cfg {

class MainConfiguration
{
 public:
  DISABLE_EVIL_CONSTRUCTOR(MainConfiguration);

  MainConfiguration();
  virtual ~MainConfiguration() = default;

  virtual void dump() = 0;

  const std::string& endpoint() const;

 protected:
  std::string theEndpoint;
};

} // namespace cfg
} // namespace okts
