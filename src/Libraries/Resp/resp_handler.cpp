#include "Resp/resp_handler.h"
#include "Resp/resp_parser.h"

#include "Storage/release_memory.h"

#include <absl/container/flat_hash_map.h>

#include <glog/logging.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>

namespace okts::resp {

namespace {

// Maximum keyword length we accept for stack-buffer upper-casing.
// Covers every RESP command (longest is SUNIONSTORE = 11) and every
// option keyword passed through this helper (LEFT/RIGHT, BEFORE/AFTER,
// COUNT, LIMIT, RANK, MAXLEN). 16 leaves headroom.
constexpr size_t kMaxCmdLen = 16;

// Upper-case `aIn` into `aBuf` and return a string_view over it.
// Returns std::string_view{} on overflow (caller treats as unknown).
// Allocation-free — caller owns the storage. Used by the dispatch
// fallback when an unknown raw-case command needs a second lookup.
std::string_view toUpperStack(std::string_view aIn,
                              char (&aBuf)[kMaxCmdLen]) {
  if (aIn.size() > kMaxCmdLen) {
    return {};
  }
  for (size_t i = 0; i < aIn.size(); ++i) {
    aBuf[i] = static_cast<char>(
        std::toupper(static_cast<unsigned char>(aIn[i])));
  }
  return std::string_view(aBuf, aIn.size());
}

// True iff `aIn` matches `aUpperLiteral` case-insensitively. The
// literal MUST already be uppercase ASCII (compile-time constant).
//
// Fast path: a direct memcmp covers the common case where the client
// already sent the keyword uppercased (redis-cli, redis-benchmark,
// most production drivers). Only a mixed-case input pays the
// per-byte upper-case loop. No allocation either way.
bool iequalsToUpper(std::string_view aIn, std::string_view aUpperLiteral) {
  if (aIn.size() != aUpperLiteral.size()) {
    return false;
  }
  if (aIn == aUpperLiteral) {
    return true;
  }
  for (size_t i = 0; i < aIn.size(); ++i) {
    if (static_cast<char>(std::toupper(static_cast<unsigned char>(aIn[i]))) !=
        aUpperLiteral[i]) {
      return false;
    }
  }
  return true;
}

// std::from_chars-based integer parsing. std::stoll/stoull throw on
// malformed input with cryptic what() messages ("stoll", "stoull")
// that bubble up to the client; std::stoull also accepts a leading
// minus sign and silently wraps "-5" to 18446744073709551611.
//
// These helpers reject leading whitespace, trailing garbage, and (for
// the unsigned variant) leading '+'/'-'. They produce Redis-style
// error strings on miss so the caller can return a typed reply.
constexpr std::string_view kErrNotInt =
    "ERR value is not an integer or out of range";

std::optional<int64_t> parseSignedInt(std::string_view aStr) {
  if (aStr.empty()) return std::nullopt;
  int64_t myValue = 0;
  auto    myRes   = std::from_chars(aStr.data(), aStr.data() + aStr.size(),
                                    myValue);
  if (myRes.ec != std::errc{} || myRes.ptr != aStr.data() + aStr.size()) {
    return std::nullopt;
  }
  return myValue;
}

// Strictly unsigned: rejects '-' and '+' prefixes (std::from_chars on
// uint64_t accepts neither, but we double-check by failing the
// negative case explicitly to avoid future-proofing surprises).
std::optional<uint64_t> parseUnsignedInt(std::string_view aStr) {
  if (aStr.empty() || aStr.front() == '-' || aStr.front() == '+') {
    return std::nullopt;
  }
  uint64_t myValue = 0;
  auto     myRes   = std::from_chars(aStr.data(), aStr.data() + aStr.size(),
                                     myValue);
  if (myRes.ec != std::errc{} || myRes.ptr != aStr.data() + aStr.size()) {
    return std::nullopt;
  }
  return myValue;
}

// Strict direction parse — only LEFT and RIGHT (case-insensitive)
// are accepted. Returns std::nullopt on anything else; callers must
// surface an "ERR syntax error" for the client.
std::optional<stor::Lists::Direction> parseDirection(const std::string& aDir) {
  if (iequalsToUpper(aDir, "LEFT")) {
    return stor::Lists::Direction::LEFT;
  }
  if (iequalsToUpper(aDir, "RIGHT")) {
    return stor::Lists::Direction::RIGHT;
  }
  return std::nullopt;
}

} // namespace

// ---- Helpers ----

std::string RespHandler::validateMinArgs(const Args& aArgs,
                                         size_t aMin,
                                         std::string_view aCommand) {
  if (aArgs.size() >= aMin) {
    return {};
  }
  return RespParser::formatError(
      "ERR wrong number of arguments for '" + std::string(aCommand) +
      "' command");
}

std::vector<std::string_view> RespHandler::extractValues(const Args& aArgs,
                                                         size_t aFrom) {
  std::vector<std::string_view> myValues;
  myValues.reserve(aArgs.size() - aFrom);
  for (size_t i = aFrom; i < aArgs.size(); ++i) {
    myValues.emplace_back(aArgs[i]);
  }
  return myValues;
}

// ---- Generic push (lpush/rpush/lpushx/rpushx) ----

std::string RespHandler::handlePush(const Args& aArgs,
                                    std::string_view aCommand,
                                    PushMethod aMethod) {
  auto myErr = validateMinArgs(aArgs, 3, aCommand);
  if (!myErr.empty()) return myErr;

  auto myValues = extractValues(aArgs, 2);
  auto mySize   = (theStorage.lists.*aMethod)(aArgs[1], myValues);
  return RespParser::formatInteger(static_cast<int64_t>(mySize));
}

// ---- Generic pop with optional count (lpop/rpop/spop/srandmember) ----

template <typename PopFunc>
std::string RespHandler::handlePopWithOptionalCount(const Args&      aArgs,
                                                    std::string_view aCommand,
                                                    bool aAllowNegativeCount,
                                                    PopFunc&&        aPopFunc) {
  auto myErr = validateMinArgs(aArgs, 2, aCommand);
  if (!myErr.empty()) return myErr;

  int64_t myCount = 1;
  bool    myMulti = false;

  if (aArgs.size() > 2) {
    auto myParsed = parseSignedInt(aArgs[2]);
    if (!myParsed) {
      return RespParser::formatError(std::string(kErrNotInt));
    }
    myCount = *myParsed;
    myMulti = true;

    // Only SRANDMEMBER accepts a negative count (selection with
    // replacement). LPOP / RPOP / SPOP set aAllowNegativeCount=false
    // and reject anything < 0.
    if (!aAllowNegativeCount && myCount < 0) {
      return RespParser::formatError(
          "ERR value is out of range, must be positive");
    }
  }

  auto myValues = aPopFunc(aArgs[1], myCount);

  if (myMulti) {
    return formatBulkStringArray(myValues);
  }
  if (myValues.empty()) {
    return RespParser::formatNullBulkString();
  }
  return RespParser::formatBulkString(myValues.front());
}

// ---- Generic multi-key set operation ----

template <typename SetOpFunc>
std::string RespHandler::handleSetOp(const Args& aArgs,
                                     std::string_view aCommand,
                                     size_t aKeysFrom,
                                     SetOpFunc&& aFunc) {
  auto myErr = validateMinArgs(aArgs, aKeysFrom + 1, aCommand);
  if (!myErr.empty()) return myErr;

  auto myKeys   = extractValues(aArgs, aKeysFrom);
  auto myResult = aFunc(myKeys);
  return formatBulkStringArray(myResult);
}

// ---- Table-driven registration ----

RespHandler::RespHandler(stor::StorageContext& aStorage)
    : theStorage(aStorage) {
}


std::string RespHandler::handle(const Args& aArgs) {
  if (aArgs.empty()) {
    return RespParser::formatError("ERR empty command");
  }

  // Build the dispatch table once at first call. Lambdas have no
  // capture, so they decay to function pointers — the whole table
  // lives in rodata. No per-connection allocations.
  // clang-format off
  static const Entry kHandlers[] = {
    // General
    {"PING",         [](RespHandler& h, const Args& a) { return h.handlePing(a); }},
    {"QUIT",         [](RespHandler& h, const Args& a) { return h.handleQuit(a); }},
    {"COMMAND",      [](RespHandler& h, const Args& a) { return h.handleCommand(a); }},
    {"CLIENT",       [](RespHandler& h, const Args& a) { return h.handleClient(a); }},
    {"SELECT",       [](RespHandler& h, const Args& a) { return h.handleSelect(a); }},
    {"INFO",         [](RespHandler& h, const Args& a) { return h.handleInfo(a); }},
    {"FLUSHDB",      [](RespHandler& h, const Args& a) { return h.handleFlush(a); }},
    {"FLUSHALL",     [](RespHandler& h, const Args& a) { return h.handleFlush(a); }},
    {"DBSIZE",       [](RespHandler& h, const Args& a) { return h.handleDbsize(a); }},

    // List push (generic)
    {"LPUSH",        [](RespHandler& h, const Args& a) { return h.handlePush(a, "lpush",  &stor::Lists::pushFront); }},
    {"RPUSH",        [](RespHandler& h, const Args& a) { return h.handlePush(a, "rpush",  &stor::Lists::pushBack); }},
    {"LPUSHX",       [](RespHandler& h, const Args& a) { return h.handlePush(a, "lpushx", &stor::Lists::pushFrontExist); }},
    {"RPUSHX",       [](RespHandler& h, const Args& a) { return h.handlePush(a, "rpushx", &stor::Lists::pushBackExist); }},

    // List pop (generic). Counts come in as int64_t; the dispatcher
    // already rejects negative for LPOP/RPOP/SPOP (allowNegative=false)
    // and only allows negative for SRANDMEMBER (selection with
    // replacement, allowNegative=true).
    {"LPOP",         [](RespHandler& h, const Args& a) {
      return h.handlePopWithOptionalCount(a, "lpop", false, [&h](const std::string& k, int64_t c) {
        return h.theStorage.lists.popFront(k, static_cast<uint64_t>(c));
      });
    }},
    {"RPOP",         [](RespHandler& h, const Args& a) {
      return h.handlePopWithOptionalCount(a, "rpop", false, [&h](const std::string& k, int64_t c) {
        return h.theStorage.lists.popBack(k, static_cast<uint64_t>(c));
      });
    }},

    // List commands (unique logic)
    {"LLEN",         [](RespHandler& h, const Args& a) { return h.handleLlen(a); }},
    {"LINDEX",       [](RespHandler& h, const Args& a) { return h.handleLindex(a); }},
    {"LINSERT",      [](RespHandler& h, const Args& a) { return h.handleLinsert(a); }},
    {"LRANGE",       [](RespHandler& h, const Args& a) { return h.handleLrange(a); }},
    {"LREM",         [](RespHandler& h, const Args& a) { return h.handleLrem(a); }},
    {"LSET",         [](RespHandler& h, const Args& a) { return h.handleLset(a); }},
    {"LTRIM",        [](RespHandler& h, const Args& a) { return h.handleLtrim(a); }},
    {"LMOVE",        [](RespHandler& h, const Args& a) { return h.handleLmove(a); }},
    {"LPOS",         [](RespHandler& h, const Args& a) { return h.handleLpos(a); }},
    {"LMPOP",        [](RespHandler& h, const Args& a) { return h.handleLmpop(a); }},

    // Set commands (generic multi-key ops)
    {"SDIFF",        [](RespHandler& h, const Args& a) {
      return h.handleSetOp(a, "sdiff", 1, [&h](const std::vector<std::string_view>& k) {
        return h.theStorage.sets.diff(k);
      });
    }},
    {"SINTER",       [](RespHandler& h, const Args& a) {
      return h.handleSetOp(a, "sinter", 1, [&h](const std::vector<std::string_view>& k) {
        return h.theStorage.sets.inter(k);
      });
    }},
    {"SUNION",       [](RespHandler& h, const Args& a) {
      return h.handleSetOp(a, "sunion", 1, [&h](const std::vector<std::string_view>& k) {
        return h.theStorage.sets.unionSets(k);
      });
    }},
    {"SMEMBERS",     [](RespHandler& h, const Args& a) {
      return h.handleSetOp(a, "smembers", 1, [&h](const std::vector<std::string_view>& k) {
        return h.theStorage.sets.members(std::string(k[0]));
      });
    }},

    // Set pop/random (generic)
    {"SPOP",         [](RespHandler& h, const Args& a) {
      return h.handlePopWithOptionalCount(a, "spop", false, [&h](const std::string& k, int64_t c) {
        return h.theStorage.sets.pop(k, static_cast<size_t>(c));
      });
    }},
    {"SRANDMEMBER",  [](RespHandler& h, const Args& a) {
      return h.handlePopWithOptionalCount(a, "srandmember", true, [&h](const std::string& k, int64_t c) {
        return h.theStorage.sets.randMember(k, c);
      });
    }},

    // Set commands (unique logic)
    {"SADD",         [](RespHandler& h, const Args& a) { return h.handleSadd(a); }},
    {"SCARD",        [](RespHandler& h, const Args& a) { return h.handleScard(a); }},
    {"SDIFFSTORE",   [](RespHandler& h, const Args& a) { return h.handleSdiffstore(a); }},
    {"SINTERCARD",   [](RespHandler& h, const Args& a) { return h.handleSintercard(a); }},
    {"SINTERSTORE",  [](RespHandler& h, const Args& a) { return h.handleSinterstore(a); }},
    {"SISMEMBER",    [](RespHandler& h, const Args& a) { return h.handleSismember(a); }},
    {"SMISMEMBER",   [](RespHandler& h, const Args& a) { return h.handleSmismember(a); }},
    {"SMOVE",        [](RespHandler& h, const Args& a) { return h.handleSmove(a); }},
    {"SREM",         [](RespHandler& h, const Args& a) { return h.handleSrem(a); }},
    {"SUNIONSTORE",  [](RespHandler& h, const Args& a) { return h.handleSunionstore(a); }},
  };
  // clang-format on

  // Index the dispatch table into a flat_hash_map once at first call.
  // string_view + transparent hash means no allocations on lookup —
  // the upper-cased stack-buffer view is matched directly. Cuts the
  // O(N) linear scan over kHandlers to O(1) and stays cheap as the
  // command set grows.
  struct StringHash {
    using is_transparent = void;
    size_t operator()(std::string_view s) const {
      return absl::Hash<std::string_view>{}(s);
    }
  };
  static const auto kIndex = []() {
    absl::flat_hash_map<std::string_view, HandlerFn, StringHash, std::equal_to<>>
        myMap;
    myMap.reserve(std::size(kHandlers));
    for (const auto& myEntry : kHandlers) {
      myMap.emplace(myEntry.name, myEntry.fn);
    }
    return myMap;
  }();

  // Most RESP clients (redis-cli, redis-benchmark, production drivers)
  // send commands already upper-cased. Try the raw view first; only
  // pay the toUpperStack pass on a miss (handles lowercase / mixed
  // case clients without slowing down the common path).
  auto myIt = kIndex.find(std::string_view(aArgs[0]));
  char myBuf[kMaxCmdLen];
  if (myIt == kIndex.end()) {
    std::string_view myUpper = toUpperStack(aArgs[0], myBuf);
    if (!myUpper.empty()) {
      myIt = kIndex.find(myUpper);
    }
  }

  if (myIt != kIndex.end()) {
    try {
      return myIt->second(*this, aArgs);
    } catch (const std::exception& e) {
      return RespParser::formatError(std::string("ERR ") + e.what());
    }
  }
  return RespParser::formatError("ERR unknown command '" + aArgs[0] + "'");
}

// ---- General commands ----

std::string RespHandler::handlePing(const Args& aArgs) {
  if (aArgs.size() > 1) {
    return RespParser::formatBulkString(aArgs[1]);
  }
  return RespParser::formatSimpleString("PONG");
}

std::string RespHandler::handleQuit(const Args&) {
  return RespParser::formatSimpleString("OK");
}

std::string RespHandler::handleCommand(const Args&) {
  return RespParser::formatEmptyArray();
}

std::string RespHandler::handleClient(const Args&) {
  return RespParser::formatSimpleString("OK");
}

std::string RespHandler::handleSelect(const Args&) {
  return RespParser::formatSimpleString("OK");
}

std::string RespHandler::handleInfo(const Args&) {
  return RespParser::formatBulkString(
      "# Server\r\noktoplus_version:0.1.0\r\n");
}

std::string RespHandler::handleFlush(const Args&) {
  // FLUSHDB / FLUSHALL: Oktoplus has a single global namespace and
  // ignores SELECT, so both drop every container in every storage type.
  // Trailing options (ASYNC / SYNC) are accepted and ignored.
  theStorage.lists.clear();
  theStorage.deques.clear();
  theStorage.vectors.clear();
  theStorage.sets.clear();
  stor::releaseMemoryToOs();
  return RespParser::formatSimpleString("OK");
}

std::string RespHandler::handleDbsize(const Args&) {
  // DBSIZE: total key count across every storage type. Oktoplus has a
  // single global namespace, so SELECT is a no-op and DBSIZE == sum of
  // hostedKeys() across all containers.
  const size_t myTotal =
      theStorage.lists.hostedKeys()   +
      theStorage.deques.hostedKeys()  +
      theStorage.vectors.hostedKeys() +
      theStorage.sets.hostedKeys();
  return RespParser::formatInteger(static_cast<int64_t>(myTotal));
}

// ---- List commands ----

std::string RespHandler::handleLlen(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 2, "llen");
  if (!myErr.empty()) return myErr;

  auto mySize = theStorage.lists.size(aArgs[1]);
  return RespParser::formatInteger(static_cast<int64_t>(mySize));
}

std::string RespHandler::handleLindex(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 3, "lindex");
  if (!myErr.empty()) return myErr;

  auto myIndex = parseSignedInt(aArgs[2]);
  if (!myIndex) {
    return RespParser::formatError(std::string(kErrNotInt));
  }
  auto myResult = theStorage.lists.index(aArgs[1], *myIndex);
  if (myResult) {
    return RespParser::formatBulkString(myResult.value());
  }
  return RespParser::formatNullBulkString();
}

std::string RespHandler::handleLinsert(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 5, "linsert");
  if (!myErr.empty()) return myErr;

  stor::Lists::Position myPosition;
  if (iequalsToUpper(aArgs[2], "BEFORE")) {
    myPosition = stor::Lists::Position::BEFORE;
  } else if (iequalsToUpper(aArgs[2], "AFTER")) {
    myPosition = stor::Lists::Position::AFTER;
  } else {
    return RespParser::formatError("ERR syntax error");
  }

  auto myResult =
      theStorage.lists.insert(aArgs[1], myPosition, aArgs[3], aArgs[4]);
  if (myResult) {
    return RespParser::formatInteger(myResult.value());
  }
  return RespParser::formatInteger(0);
}

std::string RespHandler::handleLrange(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 4, "lrange");
  if (!myErr.empty()) return myErr;

  auto myStart = parseSignedInt(aArgs[2]);
  auto myStop  = parseSignedInt(aArgs[3]);
  if (!myStart || !myStop) {
    return RespParser::formatError(std::string(kErrNotInt));
  }
  auto myValues = theStorage.lists.range(aArgs[1], *myStart, *myStop);
  return formatBulkStringArray(myValues);
}

std::string RespHandler::handleLrem(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 4, "lrem");
  if (!myErr.empty()) return myErr;

  auto myCount = parseSignedInt(aArgs[2]);
  if (!myCount) {
    return RespParser::formatError(std::string(kErrNotInt));
  }
  auto myRemoved = theStorage.lists.remove(aArgs[1], *myCount, aArgs[3]);
  return RespParser::formatInteger(static_cast<int64_t>(myRemoved));
}

std::string RespHandler::handleLset(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 4, "lset");
  if (!myErr.empty()) return myErr;

  auto myIndex = parseSignedInt(aArgs[2]);
  if (!myIndex) {
    return RespParser::formatError(std::string(kErrNotInt));
  }
  auto myStatus = theStorage.lists.set(aArgs[1], *myIndex, aArgs[3]);
  switch (myStatus) {
    case stor::Lists::Status::OK:
      return RespParser::formatSimpleString("OK");
    case stor::Lists::Status::OUT_OF_RANGE:
      return RespParser::formatError("ERR index out of range");
    case stor::Lists::Status::NOT_FOUND:
      return RespParser::formatError("ERR no such key");
  }
  return RespParser::formatError("ERR internal error");
}

std::string RespHandler::handleLtrim(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 4, "ltrim");
  if (!myErr.empty()) return myErr;

  auto myStart = parseSignedInt(aArgs[2]);
  auto myStop  = parseSignedInt(aArgs[3]);
  if (!myStart || !myStop) {
    return RespParser::formatError(std::string(kErrNotInt));
  }
  theStorage.lists.trim(aArgs[1], *myStart, *myStop);
  return RespParser::formatSimpleString("OK");
}

std::string RespHandler::handleLmove(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 5, "lmove");
  if (!myErr.empty()) return myErr;

  auto mySrcDir  = parseDirection(aArgs[3]);
  auto myDestDir = parseDirection(aArgs[4]);
  if (!mySrcDir || !myDestDir) {
    return RespParser::formatError("ERR syntax error");
  }

  auto myResult =
      theStorage.lists.move(aArgs[1], aArgs[2], *mySrcDir, *myDestDir);
  if (myResult) {
    return RespParser::formatBulkString(myResult.value());
  }
  return RespParser::formatNullBulkString();
}

std::string RespHandler::handleLpos(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 3, "lpos");
  if (!myErr.empty()) return myErr;

  const auto& myKey   = aArgs[1];
  const auto& myValue = aArgs[2];

  int64_t  myRank   = 1;
  uint64_t myCount  = 1;
  uint64_t myMaxLen = 0;
  bool     myMulti  = false;

  for (size_t i = 3; i + 1 < aArgs.size(); i += 2) {
    if (iequalsToUpper(aArgs[i], "RANK")) {
      auto myParsed = parseSignedInt(aArgs[i + 1]);
      if (!myParsed) {
        return RespParser::formatError(std::string(kErrNotInt));
      }
      // Redis: RANK 0 is rejected with a typed error.
      if (*myParsed == 0) {
        return RespParser::formatError("ERR RANK can't be zero: use 1 to "
                                       "start from the first match going "
                                       "forward, or -1 from the last match "
                                       "going backward.");
      }
      myRank = *myParsed;
    } else if (iequalsToUpper(aArgs[i], "COUNT")) {
      auto myParsed = parseUnsignedInt(aArgs[i + 1]);
      if (!myParsed) {
        return RespParser::formatError(std::string(kErrNotInt));
      }
      myCount = *myParsed;
      myMulti = true;
    } else if (iequalsToUpper(aArgs[i], "MAXLEN")) {
      auto myParsed = parseUnsignedInt(aArgs[i + 1]);
      if (!myParsed) {
        return RespParser::formatError(std::string(kErrNotInt));
      }
      myMaxLen = *myParsed;
    }
  }

  auto myPositions =
      theStorage.lists.position(myKey, myValue, myRank, myCount, myMaxLen);

  if (myMulti) {
    std::vector<std::string> myFormatted;
    for (auto myPos : myPositions) {
      myFormatted.push_back(
          RespParser::formatInteger(static_cast<int64_t>(myPos)));
    }
    return RespParser::formatArray(myFormatted);
  }

  if (myPositions.empty()) {
    return RespParser::formatNullBulkString();
  }
  return RespParser::formatInteger(
      static_cast<int64_t>(myPositions.front()));
}

std::string RespHandler::handleLmpop(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 4, "lmpop");
  if (!myErr.empty()) return myErr;

  auto myNumKeysOpt = parseUnsignedInt(aArgs[1]);
  if (!myNumKeysOpt) {
    return RespParser::formatError(std::string(kErrNotInt));
  }
  // Bound numKeys to what the rest of the request can plausibly hold.
  // Without this an attacker-supplied uint64 wraps the
  // `2 + myNumKeys + 1` size_t arithmetic below and the subsequent
  // `aArgs[2 + myNumKeys]` indexing reads OOB.
  if (*myNumKeysOpt == 0 || *myNumKeysOpt > aArgs.size() ||
      *myNumKeysOpt > aArgs.size() - 3) {
    return RespParser::formatError("ERR syntax error");
  }
  const uint64_t myNumKeys = *myNumKeysOpt;

  auto myDirection = parseDirection(aArgs[2 + myNumKeys]);
  if (!myDirection) {
    return RespParser::formatError("ERR syntax error");
  }

  uint64_t myCount = 1;
  size_t   myIdx   = 3 + myNumKeys;
  if (myIdx + 1 < aArgs.size() && iequalsToUpper(aArgs[myIdx], "COUNT")) {
    auto myParsed = parseUnsignedInt(aArgs[myIdx + 1]);
    if (!myParsed) {
      return RespParser::formatError(std::string(kErrNotInt));
    }
    myCount = *myParsed;
  }

  // LMPOP semantics: return the first key that yielded values, plus its
  // popped values, as a 2-element array [key, [values]]. Null when none.
  for (uint64_t i = 0; i < myNumKeys; ++i) {
    const auto& myKey = aArgs[2 + i];
    auto        myValues =
        *myDirection == stor::Lists::Direction::LEFT
            ? theStorage.lists.popFront(myKey, myCount)
            : theStorage.lists.popBack(myKey, myCount);
    if (!myValues.empty()) {
      return RespParser::formatArray(
          {RespParser::formatBulkString(myKey),
           formatBulkStringArray(myValues)});
    }
  }
  return RespParser::formatNullBulkString();
}

// ---- Set commands ----

std::string RespHandler::handleSadd(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 3, "sadd");
  if (!myErr.empty()) return myErr;

  auto myValues = extractValues(aArgs, 2);
  auto mySize   = theStorage.sets.add(aArgs[1], myValues);
  return RespParser::formatInteger(static_cast<int64_t>(mySize));
}

std::string RespHandler::handleScard(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 2, "scard");
  if (!myErr.empty()) return myErr;

  auto mySize = theStorage.sets.cardinality(aArgs[1]);
  return RespParser::formatInteger(static_cast<int64_t>(mySize));
}

std::string RespHandler::handleSdiffstore(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 3, "sdiffstore");
  if (!myErr.empty()) return myErr;

  auto myKeys = extractValues(aArgs, 2);
  auto mySize = theStorage.sets.diffStore(aArgs[1], myKeys);
  return RespParser::formatInteger(static_cast<int64_t>(mySize));
}

std::string RespHandler::handleSintercard(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 3, "sintercard");
  if (!myErr.empty()) return myErr;

  auto myNumKeysOpt = parseUnsignedInt(aArgs[1]);
  if (!myNumKeysOpt) {
    return RespParser::formatError(std::string(kErrNotInt));
  }
  // Bound numKeys to the request size to prevent the OOB index read
  // and the oversized myKeys.reserve() call below.
  if (*myNumKeysOpt == 0 || *myNumKeysOpt > aArgs.size() - 2) {
    return RespParser::formatError("ERR syntax error");
  }
  const uint64_t myNumKeys = *myNumKeysOpt;

  std::vector<std::string_view> myKeys;
  myKeys.reserve(myNumKeys);
  for (uint64_t i = 0; i < myNumKeys; ++i) {
    myKeys.emplace_back(aArgs[2 + i]);
  }

  uint64_t myLimit = 0;
  size_t   myIdx   = 2 + myNumKeys;
  if (myIdx + 1 < aArgs.size() && iequalsToUpper(aArgs[myIdx], "LIMIT")) {
    auto myParsed = parseUnsignedInt(aArgs[myIdx + 1]);
    if (!myParsed) {
      return RespParser::formatError(std::string(kErrNotInt));
    }
    myLimit = *myParsed;
  }

  auto myResult = theStorage.sets.inter(myKeys);
  auto myCard   = myResult.size();
  if (myLimit > 0 && myCard > myLimit) {
    myCard = myLimit;
  }
  return RespParser::formatInteger(static_cast<int64_t>(myCard));
}

std::string RespHandler::handleSinterstore(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 3, "sinterstore");
  if (!myErr.empty()) return myErr;

  auto myKeys = extractValues(aArgs, 2);
  auto mySize = theStorage.sets.interStore(aArgs[1], myKeys);
  return RespParser::formatInteger(static_cast<int64_t>(mySize));
}

std::string RespHandler::handleSismember(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 3, "sismember");
  if (!myErr.empty()) return myErr;

  bool myResult = theStorage.sets.isMember(aArgs[1], aArgs[2]);
  return RespParser::formatInteger(myResult ? 1 : 0);
}

std::string RespHandler::handleSmismember(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 3, "smismember");
  if (!myErr.empty()) return myErr;

  std::vector<std::string> myValues(aArgs.begin() + 2, aArgs.end());
  auto myResults = theStorage.sets.misMember(aArgs[1], myValues);

  std::vector<std::string> myFormatted;
  myFormatted.reserve(myResults.size());
  for (bool myVal : myResults) {
    myFormatted.push_back(RespParser::formatInteger(myVal ? 1 : 0));
  }
  return RespParser::formatArray(myFormatted);
}

std::string RespHandler::handleSmove(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 4, "smove");
  if (!myErr.empty()) return myErr;

  bool myMoved = theStorage.sets.moveMember(aArgs[1], aArgs[2], aArgs[3]);
  return RespParser::formatInteger(myMoved ? 1 : 0);
}

std::string RespHandler::handleSrem(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 3, "srem");
  if (!myErr.empty()) return myErr;

  auto myValues  = extractValues(aArgs, 2);
  auto myRemoved = theStorage.sets.remove(aArgs[1], myValues);
  return RespParser::formatInteger(static_cast<int64_t>(myRemoved));
}

std::string RespHandler::handleSunionstore(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 3, "sunionstore");
  if (!myErr.empty()) return myErr;

  auto myKeys = extractValues(aArgs, 2);
  auto mySize = theStorage.sets.unionStore(aArgs[1], myKeys);
  return RespParser::formatInteger(static_cast<int64_t>(mySize));
}

} // namespace okts::resp
