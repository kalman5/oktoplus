#pragma once

#include <glog/logging.h>
#include <google/protobuf/stubs/common.h>

namespace okts {
namespace sup {

class GoogleRaii final
{
 public:
  GoogleRaii(const char* argv0, const bool aGlog, const bool aProtobuf)
      : theGlog(aGlog)
      , theProtobuf(aProtobuf) {
    if (aGlog) {
      ::google::InitGoogleLogging(argv0);
      ::google::LogToStderr();
    }
  }

  /// Clean-up static stuff in glog and protobuf.
  ~GoogleRaii() {
    if (theGlog) {
      ::google::ShutdownGoogleLogging();
    }
    if (theProtobuf) {
      ::google::protobuf::ShutdownProtobufLibrary();
    }
  }

 private:
  const bool theGlog;
  const bool theProtobuf;
};

} // namespace sup
} // namespace okts
