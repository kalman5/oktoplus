#pragma once

#define DISABLE_EVIL_CONSTRUCTOR(class_name)                                   \
  class_name(const class_name&) = delete;                                      \
  class_name& operator=(const class_name&) = delete

