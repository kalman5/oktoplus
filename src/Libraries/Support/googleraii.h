#pragma once

#include <glog/logging.h>

// The protobuf shutdown hook is only compiled in when the build
// links protobuf (i.e. OKTOPLUS_WITH_GRPC=ON). Otherwise the
// `aProtobuf` ctor flag is accepted but ignored, so callers don't
// need to know which build flavour they're in.
#ifdef OKTOPLUS_WITH_GRPC
#include <google/protobuf/stubs/common.h>
#endif

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
#ifdef OKTOPLUS_WITH_GRPC
    if (theProtobuf) {
      ::google::protobuf::ShutdownProtobufLibrary();
    }
#else
    (void)theProtobuf;
#endif
  }

 private:
  const bool theGlog;
  const bool theProtobuf;
};

} // namespace sup
} // namespace okts
