#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <absl/container/flat_hash_set.h>

#include "Storage/genericcontainer.h"
#include "Support/noncopyable.h"

namespace okts::stor {

class Sets : public GenericContainer<absl::flat_hash_set<std::string>>
{
  using Container = absl::flat_hash_set<std::string>;
  using Base      = GenericContainer<Container>;

 public:
  DISABLE_EVIL_CONSTRUCTOR(Sets);

  Sets();

  size_t add(const std::string&                   aName,
             const std::vector<std::string_view>& aValues);

  size_t cardinality(const std::string& aName) const;

  Container diff(const std::vector<std::string_view>& aNames);

  size_t diffStore(const std::string&                   aDestination,
                   const std::vector<std::string_view>& aNames);

  Container inter(const std::vector<std::string_view>& aNames);

  size_t interStore(const std::string&                   aDestination,
                    const std::vector<std::string_view>& aNames);

  bool isMember(const std::string& aName, const std::string& aValue) const;

  std::vector<bool> misMember(const std::string&              aName,
                              const std::vector<std::string>& aValues) const;

  Container members(const std::string& aName) const;

  // Streaming variant of members(): invokes
  //
  //   aOnCardinality(size)   exactly once, with the size the
  //                          following iteration will yield, and
  //   aOnMember(string_view) once per member,
  //
  // both under the per-key lock so the caller can pre-size an
  // output buffer (SMEMBERS' RESP reply, mainly) against a
  // cardinality that matches the iteration -- no race with
  // concurrent SADD/SREM, no intermediate Container copy. Returns
  // the cardinality.
  //
  // Both callbacks run while the per-key inner mutex is held, so
  // they must not block on storage or take other long-held locks.
  // Treat them like hot-path callbacks.
  size_t forEachMember(const std::string&                    aName,
                       std::function<void(size_t)>           aOnCardinality,
                       std::function<void(std::string_view)> aOnMember) const;

  bool moveMember(const std::string& aSource,
                  const std::string& aDestination,
                  const std::string& aValue);

  std::vector<std::string> pop(const std::string& aName, size_t aCount);

  std::vector<std::string>
  randMember(const std::string& aName, int64_t aCount) const;

  size_t remove(const std::string&                   aName,
                const std::vector<std::string_view>& aValues);

  Container unionSets(const std::vector<std::string_view>& aNames);

  size_t unionStore(const std::string&                   aDestination,
                    const std::vector<std::string_view>& aNames);

 private:
  // Serialises every cross-key SMOVE to avoid the L1<->L2 deadlock
  // (analogous to SequenceContainer::theMoveMutex). moveMember holds
  // the source's per-key inner mutex AND the destination's per-key
  // inner mutex simultaneously (via a nested performOnNew inside the
  // performOnExisting lambda); two SMOVEs in opposite directions
  // would otherwise deadlock-spin under the try-lock-retry protocol.
  std::mutex theMoveMutex;
};

} // namespace okts::stor
