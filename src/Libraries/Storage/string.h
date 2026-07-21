#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace okts::stor {

// 16-byte SSO-or-heap string. A stripped-down replacement for
// std::string in the per-key container slots:
//
//   - sizeof = 16 bytes (vs libstdc++ std::string's 32). Doubles
//     cache-line density during list iteration.
//   - SSO holds up to 15 bytes inline.
//   - For >15 bytes: one heap allocation of exactly `size` bytes,
//     no NUL terminator, no capacity slack -- which lets jemalloc
//     serve from a smaller size class than std::string's
//     `capacity+1` rounding does.
//
// Why not std::variant<sso_repr, heap_repr>?
//   Once we account for the variant's discriminator + the pointer
//   alignment the heap variant pulls in, the smallest std::variant
//   we could build of a 15-byte inline buffer and a {char*, uint32_t}
//   pair lands at ~24 bytes -- which blows the 16-byte budget that
//   gives okts::stor::string its raison d'etre. We use a plain
//   discriminated layout instead, with the discriminator deliberately
//   placed OUTSIDE the union (see `mode_` below) so reads of the
//   discriminator never cross-alias an "active" union member.
//
// Why a single byte buffer + memcpy instead of an explicit union?
//   Standard C++ leaves union access through the "wrong" member as
//   undefined except for common-initial-sequence cases that don't
//   apply to us. The 15-byte inline_ buffer carries either the SSO
//   characters or a memcpy'd {char*, uint32_t} pair when in heap
//   mode; either layout is read out via std::memcpy, which is always
//   defined for trivially-copyable types.
//
// API surface kept narrow on purpose -- only what the storage path
// exercises:
//   - construction / assignment from std::string_view
//   - copy / move / dtor
//   - data() / size() / empty()
//   - implicit conversion to std::string_view (so std::find with a
//     std::string predicate works)
//   - operator==(string, string_view); std::string compares via the
//     implicit string_view conversion. C++20's reversed-candidate
//     rule generates the (lhs, rhs) and (rhs, lhs) symmetric forms.
//   - explicit toString() and a free into_std_string() helper used
//     at the few API boundaries that still hand out std::string.
//   - swap (free swap for ADL).
//
// What we deliberately don't support: operator+, +=, push_back,
// append, .c_str(), operator<, find, substr, replace. None are
// exercised by the storage path; if a future caller needs them
// we'll add narrowly.
class string {
 public:
  string() noexcept : theMode(0) {}

  /*implicit*/ string(std::string_view aValue) {
    initFrom(aValue);
  }

  // Direct ctor from std::string. C++ allows only one user-defined
  // conversion per implicit conversion sequence, so without this
  // overload `lazy_emplace(..., aName)` (aName a std::string&)
  // fails to construct a string key: it would need
  // std::string -> string_view -> okts::stor::string, two user-
  // defined steps. Providing this ctor collapses the path.
  /*implicit*/ string(const std::string& aValue)
      : string(std::string_view(aValue)) {}

  // Direct ctor from a C string literal. Without it, `okts::stor::string("hi")`
  // is ambiguous between `string(string_view)` (via string_view's
  // ctor from char*) and `string(const std::string&)` (via std::string's
  // ctor from char*) -- both one user-defined conversion. This
  // exact-match overload picks the winner.
  /*implicit*/ string(const char* aValue)
      : string(std::string_view(aValue)) {}

  string(const string& aOther) {
    initFrom(static_cast<std::string_view>(aOther));
  }

  string(string&& aOther) noexcept
      : theMode(aOther.theMode) {
    std::memcpy(theInline, aOther.theInline, sizeof(theInline));
    // Reset source to empty SSO so its dtor doesn't free our
    // (just-transferred) heap extent.
    aOther.theMode = 0;
  }

  ~string() {
    freeIfHeap();
  }

  string& operator=(const string& aOther) {
    if (this != &aOther) {
      freeIfHeap();
      initFrom(static_cast<std::string_view>(aOther));
    }
    return *this;
  }

  string& operator=(string&& aOther) noexcept {
    if (this != &aOther) {
      freeIfHeap();
      std::memcpy(theInline, aOther.theInline, sizeof(theInline));
      theMode = aOther.theMode;
      aOther.theMode = 0;
    }
    return *this;
  }

  // string_view assignment -- the surface SequenceContainer::set
  // exercises (`*it = aValue;` where aValue is a std::string from
  // the public API; std::string converts implicitly to string_view).
  string& operator=(std::string_view aRhs) {
    freeIfHeap();
    initFrom(aRhs);
    return *this;
  }

  std::size_t size() const noexcept {
    if (isHeap()) {
      std::uint32_t mySize;
      std::memcpy(&mySize, theInline + sizeof(char*), sizeof(mySize));
      return mySize;
    }
    return theMode;
  }

  bool empty() const noexcept { return size() == 0; }

  const char* data() const noexcept {
    if (isHeap()) {
      char* myPtr;
      std::memcpy(&myPtr, theInline, sizeof(myPtr));
      return myPtr;
    }
    return theInline;
  }

  /*implicit*/ operator std::string_view() const noexcept {
    return {data(), size()};
  }

  std::string toString() const {
    return {data(), size()};
  }

  void swap(string& aOther) noexcept {
    char myTmp[sizeof(theInline)];
    std::memcpy(myTmp, theInline, sizeof(theInline));
    std::memcpy(theInline, aOther.theInline, sizeof(theInline));
    std::memcpy(aOther.theInline, myTmp, sizeof(theInline));
    std::swap(theMode, aOther.theMode);
  }

  // Hidden friends: defined in-class so they are found only via
  // ADL on `okts::stor::string`. The string_view overload covers
  // std::string comparisons via std::string's implicit conversion
  // to string_view; C++20's reversed-candidate rule generates the
  // (lhs, rhs) and (rhs, lhs) forms from one definition.
  friend bool operator==(const string&    aLhs,
                         std::string_view aRhs) noexcept {
    return std::string_view(aLhs) == aRhs;
  }
  friend bool operator==(const string& aLhs,
                         const string& aRhs) noexcept {
    return std::string_view(aLhs) == std::string_view(aRhs);
  }
  // Direct overload for std::string. Without this, `string == std::string`
  // is ambiguous between (string, string_view) + std::string -> string_view
  // and (string, string) + std::string -> string -- both one user-defined
  // conversion. This exact-match overload picks the winner with zero
  // conversions; C++20's reversed-candidate rule covers the swapped form.
  friend bool operator==(const string&      aLhs,
                         const std::string& aRhs) noexcept {
    return std::string_view(aLhs) == std::string_view(aRhs);
  }

  // Abseil hash hook. absl::flat_hash_map<string, ...> finds this
  // via ADL and combines our content into the caller's hash state
  // through the string_view view. Goes through string_view so a
  // heterogeneous lookup with std::string_view or std::string
  // produces the same hash bits.
  template <typename H>
  friend H AbslHashValue(H aH, const string& aS) {
    return H::combine(std::move(aH), std::string_view(aS));
  }

 private:
  static constexpr std::uint8_t kHeapMarker = 0xFF;
  // Threshold above which we bypass jemalloc's tcache. Large values
  // (e.g. 1024B in the memory benchmark) allocated via tcache sit
  // in the freeing thread's cache after FLUSHALL and cause RSS to
  // stay ~7 MiB above baseline even after arena.4096.purge. Bypassing
  // tcache for big strings makes them go straight to arena and get
  // purged immediately, while keeping tcache for small sizes where
  // it gives 6x throughput on concurrent workloads.
  static constexpr std::size_t kLargeThreshold = 512;
  static constexpr int kMallocxTcacheNone = 256; // MALLOCX_TCACHE(-1) = (1<<8)

  // Layout: 15-byte inline buffer + 1-byte discriminator. The
  // alignas(8) guarantees the heap pointer we memcpy into the front
  // of `theInline` is naturally aligned for a load via std::memcpy
  // back into a `char*` -- the read itself is byte-wise so alignment
  // is technically optional, but pre-aligning lets the compiler
  // collapse the memcpy into a plain mov.
  //
  // theMode values:
  //   0..15 = inline length (the bytes of the SSO content live in
  //           theInline[0..theMode-1])
  //   0xFF  = heap mode:
  //             theInline[0..7]  = char* heap pointer (memcpy in/out)
  //             theInline[8..11] = uint32_t size      (memcpy in/out)
  //             theInline[12..14] = unused
  alignas(8) char     theInline[15];
  std::uint8_t        theMode;

  static_assert(sizeof(theInline) + sizeof(theMode) == 16,
                "okts::stor::string must be exactly 16 bytes");

  bool isHeap() const noexcept { return theMode == kHeapMarker; }

  // jemalloc helpers dlsym'd so binary still runs with glibc malloc.
  static void* mallocNoTcache(std::size_t aSize) {
    using Fn = void* (*)(std::size_t, int);
    static auto sFn = reinterpret_cast<Fn>(dlsym(RTLD_DEFAULT, "je_mallocx"));
    if (!sFn) sFn = reinterpret_cast<Fn>(dlsym(RTLD_DEFAULT, "mallocx"));
    if (!sFn) return nullptr;
    return sFn(aSize, kMallocxTcacheNone);
  }
  static void freeNoTcache(void* aPtr, std::size_t aSize) {
    // Prefer sdallocx(ptr, size, flags) which skips size lookup; fall back to dallocx.
    using SdFn = void (*)(void*, std::size_t, int);
    using DFn = void (*)(void*, int);
    static auto sSd = reinterpret_cast<SdFn>(dlsym(RTLD_DEFAULT, "je_sdallocx"));
    if (!sSd) sSd = reinterpret_cast<SdFn>(dlsym(RTLD_DEFAULT, "sdallocx"));
    if (sSd) {
      sSd(aPtr, aSize, kMallocxTcacheNone);
      return;
    }
    static auto sD = reinterpret_cast<DFn>(dlsym(RTLD_DEFAULT, "je_dallocx"));
    if (!sD) sD = reinterpret_cast<DFn>(dlsym(RTLD_DEFAULT, "dallocx"));
    if (sD) {
      sD(aPtr, kMallocxTcacheNone);
      return;
    }
    // Last resort: plain free (may go via tcache but better than leak).
    ::operator delete[](aPtr);
  }

  void freeIfHeap() noexcept {
    if (isHeap()) {
      char* myPtr;
      std::memcpy(&myPtr, theInline, sizeof(myPtr));
      std::uint32_t mySize;
      std::memcpy(&mySize, theInline + sizeof(char*), sizeof(mySize));
      if (mySize > kLargeThreshold) {
        // Try no-tcache free; if jemalloc not linked it falls back to delete[] inside helper.
        // To know if we used mallocx, we need to know if alloc was via mallocx. We track via size:
        // large allocs always use mallocx, so always use no-tcache free path which will handle fallback.
        // For safety, check if mallocx symbol exists by trying freeNoTcache which itself falls back.
        // If the pointer was allocated via new[], freeNoTcache's fallback delete[] will still be correct
        // for glibc but may mismatch for jemalloc's mallocx? mallocx memory must be freed via dallocx/sdallocx,
        // not delete[]. So we need to know allocation method. We store a marker: for large, we always
        // allocate via mallocx if available, so we free via dallocx. If mallocx not available, we allocated
        // via new[] and should delete[].
        // Detect jemalloc availability via presence of je_mallocx symbol.
        using Fn = void* (*)(std::size_t, int);
        static auto sM = reinterpret_cast<Fn>(dlsym(RTLD_DEFAULT, "je_mallocx"));
        if (!sM) sM = reinterpret_cast<Fn>(dlsym(RTLD_DEFAULT, "mallocx"));
        if (sM) {
          freeNoTcache(myPtr, mySize);
        } else {
          delete[] myPtr;
        }
      } else {
        delete[] myPtr;
      }
    }
  }

  void initFrom(std::string_view aSv) {
    if (aSv.size() <= 15) {
      if (!aSv.empty()) {
        std::memcpy(theInline, aSv.data(), aSv.size());
      }
      theMode = static_cast<std::uint8_t>(aSv.size());
    } else {
      char* myBuf = nullptr;
      if (aSv.size() > kLargeThreshold) {
        myBuf = static_cast<char*>(mallocNoTcache(aSv.size()));
      }
      if (!myBuf) {
        // Fallback to new[] (works with glibc and jemalloc tcache path)
        myBuf = new char[aSv.size()];
      }
      std::memcpy(myBuf, aSv.data(), aSv.size());
      std::memcpy(theInline, &myBuf, sizeof(myBuf));
      std::uint32_t mySize = static_cast<std::uint32_t>(aSv.size());
      std::memcpy(theInline + sizeof(myBuf), &mySize, sizeof(mySize));
      theMode = kHeapMarker;
    }
  }
};

// Free swap for ADL.
inline void swap(string& aA, string& aB) noexcept { aA.swap(aB); }

// Helper for the SequenceContainer pop / drain paths which return
// std::optional<std::string> / std::vector<std::string> at the
// public API boundary. Lets the same lambda body work for both
// std::string-backed (Vectors / Deques today) and string-backed
// (Lists) containers without an `if constexpr` at every call site.
//
// std::string overloads move when possible (no byte copy); string
// overloads always copy bytes (std::string can't adopt our heap
// because allocators differ -- okts::stor::string uses operator
// new[] / delete[], std::string uses std::allocator with its own
// internal contract). The bytes-copy is paid only on the read path,
// bounded by the value's own size.
inline std::string into_std_string(std::string&& aValue) noexcept {
  return std::move(aValue);
}
inline std::string into_std_string(const std::string& aValue) {
  return aValue;
}
inline std::string into_std_string(const string& aValue) {
  return std::string(aValue.data(), aValue.size());
}
// Trailing rvalue overload so `into_std_string(std::move(slot))`
// resolves unambiguously even when `slot` is an okts::stor::string.
inline std::string into_std_string(string&& aValue) {
  return std::string(aValue.data(), aValue.size());
}

} // namespace okts::stor

// std::hash specialization. Hashing through std::string_view keeps
// us bit-identical with std::hash<std::string_view> and
// std::hash<std::string>, so heterogeneous lookups in containers
// keyed on okts::stor::string but probed with a std::string_view /
// std::string find the same bucket.
namespace std {
template <>
struct hash<::okts::stor::string> {
  std::size_t operator()(const ::okts::stor::string& aS) const noexcept {
    return std::hash<std::string_view>{}(std::string_view(aS));
  }
};
} // namespace std
