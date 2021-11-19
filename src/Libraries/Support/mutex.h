#pragma once

#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <absl/base/internal/thread_annotations.h>

namespace okts {
namespace sup {

#if defined(__clang__)

#define THREAD_ANNOTATION_ATTRIBUTE(x) __attribute__((x))

#else

#define THREAD_ANNOTATION_ATTRIBUTE(x)

#endif

//#define SCOPED_LOCKABLE THREAD_ANNOTATION_ATTRIBUTE(scoped_lockable)
#define MUTEX_CAPABILITY THREAD_ANNOTATION_ATTRIBUTE(capability("mutex"))
#define ACQUIRE_CAPABILITY(m) THREAD_ANNOTATION_ATTRIBUTE(acquire_capability(m))
#define RELEASE_CAPABILITY THREAD_ANNOTATION_ATTRIBUTE(release_capability())

//#define GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE(guarded_by(x))
//#define PT_GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE(pt_guarded_by(x))

using Mutex          MUTEX_CAPABILITY = boost::mutex;
using RecursiveMutex MUTEX_CAPABILITY = boost::recursive_mutex;
using SharedMutex    MUTEX_CAPABILITY = boost::shared_mutex;

template <class T>
class SCOPED_LOCKABLE unique_lock
{
 public:
  explicit unique_lock(T& aMutex) ACQUIRE_CAPABILITY(aMutex)
      : theLock(aMutex) {
  }

  ~unique_lock() RELEASE_CAPABILITY = default;

 private:
  boost::unique_lock<T> theLock;
};

template <class T>
class SCOPED_LOCKABLE lock_guard
{

 public:
  explicit lock_guard(T& aMutex) ACQUIRE_CAPABILITY(aMutex)
      : theLock(aMutex) {
  }

  ~lock_guard() RELEASE_CAPABILITY = default;

 private:
  boost::lock_guard<T> theLock;
};

} // namespace sup
} // namespace okts
