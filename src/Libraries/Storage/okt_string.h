#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

namespace okts::stor {

// 16-byte SSO-or-heap string. Stripped-down replacement for
// std::string in our devector<...> value slots:
//
//   - sizeof = 16 bytes  (vs libstdc++ std::string's 32 bytes; saves
//     16 bytes per slot in the per-key container).
//   - SSO holds up to 15 bytes inline; the 16th byte doubles as both
//     "inline length" (0..15) and the "I'm in heap mode" sentinel
//     (0xFF). std::string can't fit 15 inline bytes in libstdc++
//     because it also reserves space for capacity tracking; we can,
//     because our slots are write-once at insert time and don't need
//     capacity headroom.
//   - For >15 bytes: one heap allocation of exactly `size` bytes,
//     no capacity slack. uint32_t length cap is 4 GiB which is well
//     above the protocol's per-bulk-string limit.
//
// What we deliberately don't support (vs std::string):
//   - operator+, +=, push_back, append: slots are write-once.
//   - .c_str() / NUL termination: every read path pairs the buffer
//     with .size(); RESP serialisation never relies on NUL.
//   - operator< / find / substr / replace / ...: not used by any
//     storage path we have today.
//
// What we DO support (matches the std::string surface the storage
// path actually exercises):
//   - construction from std::string_view (implicit so emplace_back
//     from a string_view continues to work).
//   - copy + move + dtor.
//   - data() / size() / empty().
//   - implicit conversion to std::string_view (so std::find with a
//     std::string predicate works).
//   - explicit conversion back to std::string for API-boundary
//     points that still hand out std::string to the caller (LRANGE,
//     LPOP, ...).
//   - == against std::string_view, std::string, and other okt_string
//     -- the unique comparison the storage layer needs.
class okt_string {
 public:
  okt_string() noexcept {
    u_.sso.len = 0;
  }

  /*implicit*/ okt_string(std::string_view aValue) {
    initFrom(aValue.data(), aValue.size());
  }

  okt_string(const okt_string& aOther) {
    initFrom(aOther.data(), aOther.size());
  }

  okt_string(okt_string&& aOther) noexcept {
    std::memcpy(&u_, &aOther.u_, sizeof(Repr));
    // Reset source to empty SSO so its dtor does not free our extent.
    aOther.u_.sso.len = 0;
  }

  ~okt_string() {
    freeIfHeap();
  }

  okt_string& operator=(const okt_string& aOther) {
    if (this != &aOther) {
      freeIfHeap();
      initFrom(aOther.data(), aOther.size());
    }
    return *this;
  }

  okt_string& operator=(okt_string&& aOther) noexcept {
    if (this != &aOther) {
      freeIfHeap();
      std::memcpy(&u_, &aOther.u_, sizeof(Repr));
      aOther.u_.sso.len = 0;
    }
    return *this;
  }

  // string_view assignment is the surface SequenceContainer::set
  // exercises (`*it = aValue;` where aValue is a std::string from
  // the public API). Free-then-rebuild is fine here because slots
  // are written once on insert and very rarely re-assigned.
  okt_string& operator=(std::string_view aRhs) {
    freeIfHeap();
    initFrom(aRhs.data(), aRhs.size());
    return *this;
  }

  std::size_t size() const noexcept {
    return isHeap() ? u_.heap.size : u_.sso.len;
  }

  bool empty() const noexcept {
    return size() == 0;
  }

  const char* data() const noexcept {
    return isHeap() ? u_.heap.data : u_.sso.data;
  }

  /*implicit*/ operator std::string_view() const noexcept {
    return {data(), size()};
  }

  std::string toString() const {
    return {data(), size()};
  }

  // Equality: explicitly defined for the three comparators the
  // storage layer uses. Implicit conversions would otherwise pull
  // in a chain (okt_string -> string_view, std::string -> string_view)
  // that's clearer when written out.
  friend bool operator==(const okt_string& aLhs,
                         std::string_view  aRhs) noexcept {
    return std::string_view(aLhs) == aRhs;
  }
  friend bool operator==(std::string_view  aLhs,
                         const okt_string& aRhs) noexcept {
    return aLhs == std::string_view(aRhs);
  }
  friend bool operator==(const okt_string& aLhs,
                         const okt_string& aRhs) noexcept {
    return std::string_view(aLhs) == std::string_view(aRhs);
  }
  friend bool operator==(const okt_string& aLhs,
                         const std::string& aRhs) noexcept {
    return std::string_view(aLhs) == std::string_view(aRhs);
  }
  friend bool operator==(const std::string& aLhs,
                         const okt_string& aRhs) noexcept {
    return std::string_view(aLhs) == std::string_view(aRhs);
  }

 private:
  static constexpr std::uint8_t kHeapMarker = 0xFF;

  // Layout: 16 bytes total. The last byte (`sso.len` / `heap.marker`)
  // doubles as the "is this small or heap?" discriminator. The
  // marker value 0xFF is unreachable as an inline length (max 15),
  // so the discriminator never collides with a legal SSO length.
  union Repr {
    struct {
      char          data[15];
      std::uint8_t  len;     // 0..15 = inline length; 0xFF = heap mode
    } sso;
    struct {
      char*         data;
      std::uint32_t size;
      std::uint8_t  _pad[3];
      std::uint8_t  marker;  // 0xFF when in heap mode
    } heap;
    Repr() {}
  } u_;
  static_assert(sizeof(Repr) == 16,
                "okt_string must be exactly 16 bytes");

  bool isHeap() const noexcept {
    return u_.sso.len == kHeapMarker;
  }

  void freeIfHeap() noexcept {
    if (isHeap()) {
      std::free(u_.heap.data);
    }
  }

  void initFrom(const char* aData, std::size_t aSize) {
    if (aSize <= 15) {
      if (aSize > 0) {
        std::memcpy(u_.sso.data, aData, aSize);
      }
      u_.sso.len = static_cast<std::uint8_t>(aSize);
    } else {
      // No oversized check -- aSize comes from a std::string_view
      // backed by the parsed RESP frame, which is itself bounded by
      // the bulk-string parser at parse time. uint32_t fits 4 GiB,
      // well above the protocol limit.
      char* myBuf = static_cast<char*>(std::malloc(aSize));
      // OOM = abort here; same as std::string would propagate
      // bad_alloc out of the storage path. We don't have a
      // recovery story for OOM in the hot path.
      std::memcpy(myBuf, aData, aSize);
      u_.heap.data   = myBuf;
      u_.heap.size   = static_cast<std::uint32_t>(aSize);
      u_.heap.marker = kHeapMarker;
    }
  }
};

// Helper for the SequenceContainer pop / drain paths which return
// std::optional<std::string> / std::vector<std::string> at the
// public API boundary. Lets the same lambda body work for both
// std::string-backed (Vectors / Deques today) and okt_string-backed
// (Lists) containers without an `if constexpr` at every call site.
//   - std::string&& -> std::string : moves (cheap; the storage
//     slot's std::string is consumed, no byte copy).
//   - okt_string  -> std::string : copies the bytes (std::string
//     can't adopt our heap, allocators differ). Bounded by the
//     value's own size, paid only on the pop / drain path.
inline std::string into_std_string(std::string&& aValue) noexcept {
  return std::move(aValue);
}
inline std::string into_std_string(const std::string& aValue) {
  return aValue;
}
inline std::string into_std_string(const okt_string& aValue) {
  return std::string(aValue.data(), aValue.size());
}
// Trailing rvalue overload so `into_std_string(std::move(slot))`
// resolves unambiguously even when slot is an okt_string.
inline std::string into_std_string(okt_string&& aValue) {
  return std::string(aValue.data(), aValue.size());
}

} // namespace okts::stor
