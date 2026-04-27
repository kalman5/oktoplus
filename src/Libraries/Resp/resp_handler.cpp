#include "Resp/resp_handler.h"
#include "Resp/resp_parser.h"

#include "Storage/release_memory.h"

#include <absl/container/flat_hash_map.h>

#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>

#include <glog/logging.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <memory>
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

void RespHandler::setDeferredContext(boost::asio::io_context& aIo,
                                     DeferredReplySink        aSink) {
  theIo   = &aIo;
  theSink = std::move(aSink);
}

namespace {

// Dispatch table for non-blocking RESP commands. Defined once at
// program start (anonymous namespace, file-scope). Lambdas have no
// capture so they decay to function pointers and the whole table
// lives in rodata; no per-connection allocations. The lambdas call
// public RespHandler members.
//
// Blocking commands (BLPOP / BRPOP) live outside this table since
// they return std::optional<std::string>; see RespHandler::handle.
using HandlerFn = std::string (*)(RespHandler&,
                                  const std::vector<std::string>&);
struct Entry {
  std::string_view name;   // already upper-cased
  HandlerFn        fn;
};

using Args = std::vector<std::string>;

// clang-format off
const Entry kHandlers[] = {
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
    {"MEMORY",       [](RespHandler& h, const Args& a) { return h.handleMemory(a); }},

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
        return h.lists().popFront(k, static_cast<uint64_t>(c));
      });
    }},
    {"RPOP",         [](RespHandler& h, const Args& a) {
      return h.handlePopWithOptionalCount(a, "rpop", false, [&h](const std::string& k, int64_t c) {
        return h.lists().popBack(k, static_cast<uint64_t>(c));
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
        return h.sets().diff(k);
      });
    }},
    {"SINTER",       [](RespHandler& h, const Args& a) {
      return h.handleSetOp(a, "sinter", 1, [&h](const std::vector<std::string_view>& k) {
        return h.sets().inter(k);
      });
    }},
    {"SUNION",       [](RespHandler& h, const Args& a) {
      return h.handleSetOp(a, "sunion", 1, [&h](const std::vector<std::string_view>& k) {
        return h.sets().unionSets(k);
      });
    }},
    {"SMEMBERS",     [](RespHandler& h, const Args& a) { return h.handleSmembers(a); }},

    // Set pop/random (generic)
    {"SPOP",         [](RespHandler& h, const Args& a) {
      return h.handlePopWithOptionalCount(a, "spop", false, [&h](const std::string& k, int64_t c) {
        return h.sets().pop(k, static_cast<size_t>(c));
      });
    }},
    {"SRANDMEMBER",  [](RespHandler& h, const Args& a) {
      return h.handlePopWithOptionalCount(a, "srandmember", true, [&h](const std::string& k, int64_t c) {
        return h.sets().randMember(k, c);
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

struct StringHash {
  using is_transparent = void;
  size_t operator()(std::string_view s) const {
    return absl::Hash<std::string_view>{}(s);
  }
};

// O(1) lookup. string_view + transparent hash, no allocation on
// lookup. Built once at static-init by walking kHandlers.
const absl::flat_hash_map<std::string_view, HandlerFn,
                          StringHash, std::equal_to<>> kIndex = []() {
  absl::flat_hash_map<std::string_view, HandlerFn,
                      StringHash, std::equal_to<>> myMap;
  myMap.reserve(std::size(kHandlers));
  for (const auto& myEntry : kHandlers) {
    myMap.emplace(myEntry.name, myEntry.fn);
  }
  return myMap;
}();

} // namespace

std::string RespHandler::handle(const Args& aArgs) {
  if (aArgs.empty()) {
    return RespParser::formatError("ERR empty command");
  }

  // Most RESP clients (redis-cli, redis-benchmark, production drivers)
  // send commands already upper-cased. Try the raw view first; only
  // pay the toUpperStack pass on a miss (handles lowercase / mixed
  // case clients without slowing down the common path).
  auto myIt = kIndex.find(std::string_view(aArgs[0]));
  if (myIt == kIndex.end()) {
    char myBuf[kMaxCmdLen];
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

  // Blocking commands sit outside the static dispatch table because
  // their handlers return std::optional<std::string> -- empty when
  // the command suspended (the eventual reply is delivered via the
  // deferred sink). Match case-insensitively against the raw arg
  // (the table miss above didn't normalize for us if the cmd isn't
  // in the table). Linear scan is fine — only four entries. Same
  // captureless-lambda shape as kHandlers above, so the table lives
  // in rodata and no per-call allocation happens.
  using BlockingFn = std::optional<std::string> (*)(RespHandler&, const Args&);
  struct BlockingEntry {
    std::string_view name;
    BlockingFn       fn;
  };
  static constexpr BlockingEntry kBlocking[] = {
      {"BLPOP",      [](RespHandler& h, const Args& a) { return h.handleBlpop(a); }},
      {"BRPOP",      [](RespHandler& h, const Args& a) { return h.handleBrpop(a); }},
      {"BLMOVE",     [](RespHandler& h, const Args& a) { return h.handleBlmove(a); }},
      {"BLMPOP",     [](RespHandler& h, const Args& a) { return h.handleBlmpop(a); }},
      {"BRPOPLPUSH", [](RespHandler& h, const Args& a) { return h.handleBrpoplpush(a); }},
  };
  for (const auto& myEntry : kBlocking) {
    if (iequalsToUpper(aArgs[0], myEntry.name)) {
      try {
        auto myBlocking = myEntry.fn(*this, aArgs);
        // Empty optional means the handler accepted and suspended --
        // the sentinel empty string is what the Connection's read loop
        // checks to decide whether to enter the blocked state.
        return myBlocking ? *myBlocking : std::string{};
      } catch (const std::exception& e) {
        return RespParser::formatError(std::string("ERR ") + e.what());
      }
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

// MEMORY <subcommand>. Only PURGE is implemented today; other Redis
// subcommands (USAGE / STATS / MALLOC-STATS / DOCTOR) return an
// unknown-subcommand error in the same shape Redis uses.
std::string RespHandler::handleMemory(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 2, "memory");
  if (!myErr.empty()) return myErr;

  if (iequalsToUpper(aArgs[1], "PURGE")) {
    // Same hook FLUSHDB / FLUSHALL run; safe to call when there's
    // nothing to purge (jemalloc no-op) and idempotent across calls.
    stor::releaseMemoryToOs();
    return RespParser::formatSimpleString("OK");
  }

  return RespParser::formatError(
      "ERR Unknown MEMORY subcommand or wrong number of arguments for '" +
      aArgs[1] + "'");
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

std::string RespHandler::handleSmembers(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 2, "smembers");
  if (!myErr.empty()) return myErr;

  // Stream the set directly into the reply string. The cardinality
  // callback runs first under the per-key lock and pre-reserves the
  // reply buffer based on the *exact* size the iteration will emit
  // (no race with concurrent SADD/SREM, no stale `*N\r\n` header).
  // Old path went through Sets::members() (full flat_hash_set copy
  // under the lock) plus formatBulkStringArray() (two iterations
  // over the result), both unnecessary for SMEMBERS.
  std::string myReply;
  theStorage.sets.forEachMember(
      aArgs[1],
      [&myReply](size_t aCardinality) {
        // RESP overhead per element ~= "$LLLL\r\nDATA\r\n" -- the
        // 16-byte fudge covers small members and amortises across
        // the set; for very long values reserve will undershoot by
        // a small constant per element which std::string handles
        // with one geometric realloc at most.
        myReply.reserve(16 + aCardinality * 16);
        RespParser::appendArrayHeader(myReply, aCardinality);
      },
      [&myReply](std::string_view aMember) {
        RespParser::appendBulkString(myReply, aMember);
      });
  return myReply;
}

// ---- Blocking list commands -------------------------------------------

// BLPOP / BRPOP successful reply is a 2-element array of (key, value).
// On timeout Redis returns the nil array (`*-1\r\n`).
std::string RespHandler::formatBlpopReply(const std::string& aKey,
                                          const std::string& aValue) {
  std::string myReply;
  myReply.reserve(8 + aKey.size() + aValue.size() + 16);
  RespParser::appendArrayHeader(myReply, 2);
  RespParser::appendBulkString(myReply, aKey);
  RespParser::appendBulkString(myReply, aValue);
  return myReply;
}

std::string RespHandler::formatBlpopNilReply() {
  return "*-1\r\n";
}

std::optional<std::string>
RespHandler::handleBlpop(const Args& aArgs) {
  return handleBlockingPop(aArgs, "blpop", /*aFront=*/true);
}

std::optional<std::string>
RespHandler::handleBrpop(const Args& aArgs) {
  return handleBlockingPop(aArgs, "brpop", /*aFront=*/false);
}

// Common path for BLPOP / BRPOP. Single-key for v1; multi-key support
// (BLPOP key1 key2 ... timeout) is a follow-up because it requires
// registering a waiter on each key and atomically cancelling the
// others on first wake.
//
// Wire format: BLPOP key timeout
//   - timeout is a float (seconds). 0 means wait forever.
//   - Reply on hit: *2\r\n$<keylen>\r\n<key>\r\n$<vallen>\r\n<val>\r\n
//   - Reply on timeout: *-1\r\n
std::optional<std::string>
RespHandler::handleBlockingPop(const Args&     aArgs,
                               std::string_view aCommand,
                               bool             aFront) {
  auto myErr = validateMinArgs(aArgs, 3, aCommand);
  if (!myErr.empty()) return myErr;

  if (aArgs.size() > 3) {
    return RespParser::formatError(
        std::string("ERR ") + std::string(aCommand) +
        " supports only single-key form in this build");
  }

  // Last arg = timeout in seconds (float). 0 means wait indefinitely.
  double myTimeoutSecs = 0.0;
  try {
    size_t myConsumed = 0;
    myTimeoutSecs = std::stod(aArgs[2], &myConsumed);
    if (myConsumed != aArgs[2].size()) {
      throw std::invalid_argument("trailing garbage");
    }
  } catch (...) {
    return RespParser::formatError("ERR timeout is not a float or out of range");
  }
  if (myTimeoutSecs < 0) {
    return RespParser::formatError("ERR timeout is negative");
  }

  if (theIo == nullptr || !theSink) {
    // No async context (unit tests instantiate the handler without
    // wiring a deferred sink). Behave as a non-blocking pop only.
    return RespParser::formatError(
        "ERR blocking commands require an async connection context");
  }

  const std::string& myKey = aArgs[1];

  // The wake closure: invoked exactly once, either by a producer
  // transferring a value (storage drain) or by our own timeout
  // handler.
  //
  // Recovery: if the connection has disconnected by the time the
  // wake fires (or is already dead when the dispatched lambda
  // lands), the value the storage drain transferred out of the
  // list MUST be put back -- otherwise a client that disconnects
  // mid-BLPOP silently loses a pushed item. We pass an OnDead
  // recovery callable down through the sink so both layers (sink-
  // synchronous "weak ptr already expired" and dispatch-lambda
  // "socket closed before we got around to writing") can run it.
  auto mySink    = theSink;     // copy — sink may outlive this handler
  auto myKeyCopy = myKey;
  auto* myStorePtr = &theStorage.lists;
  auto myOnWake  =
      [mySink, myKeyCopy, myStorePtr, aFront](
          std::optional<std::string> aValue) {
        if (!aValue) {
          // Cancellation / timeout. Nothing was popped; nothing to
          // recover. Sink writes nil if the connection is alive,
          // no-op otherwise.
          mySink(formatBlpopNilReply(), nullptr);
          return;
        }
        // Got a value. Build the recovery callable that puts it
        // back into the source list if the dispatched lambda finds
        // the connection dead. Capture the value by copy so the
        // recovery survives the move into the sink.
        auto myValueCopy = *aValue;
        auto myOnDead =
            [myStorePtr, myKeyCopy, myValueCopy, aFront]() {
              // Re-establish the value at the side the producer
              // originally pushed it to: BLPOP wakes from LPUSH
              // (front), BRPOP from RPUSH (back).
              std::vector<std::string_view> myValues{myValueCopy};
              if (aFront) {
                myStorePtr->pushFront(myKeyCopy, myValues);
              } else {
                myStorePtr->pushBack(myKeyCopy, myValues);
              }
            };
        mySink(formatBlpopReply(myKeyCopy, *aValue), std::move(myOnDead));
      };

  okts::stor::WaiterId myId = 0;
  auto myImmediate =
      aFront
          ? theStorage.lists.tryPopFrontOrWait(myKey, myOnWake, &myId)
          : theStorage.lists.tryPopBackOrWait(myKey, myOnWake, &myId);

  if (myImmediate) {
    // Got a value inline — no waiter registered, no timer needed.
    return formatBlpopReply(myKey, *myImmediate);
  }

  // Suspended on a waiter. If there's a finite timeout, race a timer
  // against the producer wake.
  if (myTimeoutSecs > 0) {
    auto myTimer = std::make_shared<boost::asio::steady_timer>(*theIo);
    myTimer->expires_after(std::chrono::milliseconds(
        static_cast<long>(myTimeoutSecs * 1000)));
    auto& myStore = theStorage.lists;
    myTimer->async_wait(
        [myId, myKeyCopy, mySink, &myStore, myTimer](
            const boost::system::error_code& aEc) {
          if (aEc == boost::asio::error::operation_aborted) {
            return; // wake fired first; nothing to do
          }
          // Race: producer might have woken us already. cancelWaiter
          // returns true only if we still own the wake-side; in that
          // case we deliver nil. No recovery needed (timeout means
          // no value was popped).
          if (myStore.cancelWaiter(myKeyCopy, myId)) {
            mySink(formatBlpopNilReply(), nullptr);
          }
        });
  }

  return std::nullopt; // suspended
}

// BRPOPLPUSH source destination timeout
//
// Deprecated since Redis 6.2 in favour of BLMOVE; kept for clients
// that still issue it. Semantically identical to:
//   BLMOVE source destination RIGHT LEFT timeout
// so we just rewrite the args and dispatch into handleBlmove rather
// than duplicate the wake / timer / recovery scaffolding.
std::optional<std::string>
RespHandler::handleBrpoplpush(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 4, "brpoplpush");
  if (!myErr.empty()) return myErr;
  if (aArgs.size() > 4) {
    return RespParser::formatError("ERR syntax error");
  }
  Args myRewritten;
  myRewritten.reserve(6);
  myRewritten.emplace_back("BLMOVE");
  myRewritten.emplace_back(aArgs[1]);
  myRewritten.emplace_back(aArgs[2]);
  myRewritten.emplace_back("RIGHT");
  myRewritten.emplace_back("LEFT");
  myRewritten.emplace_back(aArgs[3]);
  return handleBlmove(myRewritten);
}

// LMPOP / BLMPOP successful reply: 2-element array of (key, [values]).
std::string RespHandler::formatLmpopReply(
    const std::string& aKey, const std::vector<std::string>& aValues) {
  size_t myReserved = 16 + aKey.size();
  for (const auto& myVal : aValues) {
    myReserved += 8 + myVal.size();
  }
  std::string myReply;
  myReply.reserve(myReserved);
  RespParser::appendArrayHeader(myReply, 2);
  RespParser::appendBulkString(myReply, aKey);
  RespParser::appendArrayHeader(myReply, aValues.size());
  for (const auto& myVal : aValues) {
    RespParser::appendBulkString(myReply, myVal);
  }
  return myReply;
}

// Wire format: BLMOVE source destination LEFT|RIGHT LEFT|RIGHT timeout
//   - timeout is a float (seconds). 0 means wait indefinitely.
//   - Reply on hit: $<vlen>\r\n<value>\r\n  (the moved value)
//   - Reply on timeout: $-1\r\n               (null bulk string)
//
// If the source list is non-empty when the call lands, this collapses
// to plain LMOVE under the same per-key lock and replies inline.
// Otherwise we register a BLPOP-style waiter on `source` (preferred
// side determined by aSrcDir) and complete the move in the wake
// callback by pushing the popped value into `destination`. The
// destination-side push and the source-side pop are NOT performed
// under a single lock acquisition (Redis is single-threaded so it
// gets atomicity for free; we don't), but no value is ever lost: the
// producer's drain has already removed the value from source before
// the wake fires, so the wake either successfully commits the move
// to destination or — if the connection is gone — leaves the value
// in destination as a side effect. That mirrors the LMOVE semantic
// where a successful move stays committed even if the network reply
// is lost mid-flight.
std::optional<std::string>
RespHandler::handleBlmove(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 6, "blmove");
  if (!myErr.empty()) return myErr;
  if (aArgs.size() > 6) {
    return RespParser::formatError("ERR syntax error");
  }

  auto mySrcDir  = parseDirection(aArgs[3]);
  auto myDestDir = parseDirection(aArgs[4]);
  if (!mySrcDir || !myDestDir) {
    return RespParser::formatError("ERR syntax error");
  }

  double myTimeoutSecs = 0.0;
  try {
    size_t myConsumed = 0;
    myTimeoutSecs = std::stod(aArgs[5], &myConsumed);
    if (myConsumed != aArgs[5].size()) {
      throw std::invalid_argument("trailing garbage");
    }
  } catch (...) {
    return RespParser::formatError("ERR timeout is not a float or out of range");
  }
  if (myTimeoutSecs < 0) {
    return RespParser::formatError("ERR timeout is negative");
  }

  // Try inline LMOVE first. The same-key case (source == destination)
  // is handled atomically inside Lists::move under a single inner lock.
  auto myInline =
      theStorage.lists.move(aArgs[1], aArgs[2], *mySrcDir, *myDestDir);
  if (myInline) {
    return RespParser::formatBulkString(*myInline);
  }

  if (theIo == nullptr || !theSink) {
    return RespParser::formatError(
        "ERR blocking commands require an async connection context");
  }

  const std::string& mySrc      = aArgs[1];
  const std::string& myDest     = aArgs[2];
  const bool         mySrcFront = (*mySrcDir == stor::Lists::Direction::LEFT);
  const bool         myDstFront = (*myDestDir == stor::Lists::Direction::LEFT);

  auto mySink     = theSink;
  auto myStorePtr = &theStorage.lists;
  auto mySrcCopy  = mySrc;
  auto myDestCopy = myDest;

  auto myOnWake =
      [mySink, mySrcCopy, myDestCopy, myStorePtr, mySrcFront, myDstFront](
          std::optional<std::string> aValue) {
        if (!aValue) {
          // Cancellation / timeout. No value transferred; reply nil
          // (BLMOVE timeout is a null bulk string, not the BLPOP-style
          // null array).
          mySink(RespParser::formatNullBulkString(), nullptr);
          return;
        }
        // Producer transferred a value out of the source list. Commit
        // the move by pushing into the destination list. pushFront /
        // pushBack take their own per-key locks; safe to call from this
        // wake callback (we hold no locks here).
        std::vector<std::string_view> myValues{*aValue};
        if (myDstFront) {
          myStorePtr->pushFront(myDestCopy, myValues);
        } else {
          myStorePtr->pushBack(myDestCopy, myValues);
        }
        // Reply with the moved value. No OnDead recovery: the move has
        // already committed server-side; if the connection is gone the
        // value stays in the destination list (matches Redis semantics
        // for a reply-lost-in-flight LMOVE).
        mySink(RespParser::formatBulkString(*aValue), nullptr);

        // Suppress unused-capture warnings on the no-recovery path.
        (void)mySrcCopy;
        (void)mySrcFront;
      };

  okts::stor::WaiterId myId = 0;
  auto myImmediate =
      mySrcFront
          ? theStorage.lists.tryPopFrontOrWait(mySrc, myOnWake, &myId)
          : theStorage.lists.tryPopBackOrWait(mySrc, myOnWake, &myId);

  if (myImmediate) {
    // Source got data between the move() attempt above and tryPop.
    // Apply the move semantics inline (push to dest, return value).
    std::vector<std::string_view> myValues{*myImmediate};
    if (myDstFront) {
      theStorage.lists.pushFront(myDest, myValues);
    } else {
      theStorage.lists.pushBack(myDest, myValues);
    }
    return RespParser::formatBulkString(*myImmediate);
  }

  // Suspended. Race a timer against the producer wake if requested.
  if (myTimeoutSecs > 0) {
    auto myTimer = std::make_shared<boost::asio::steady_timer>(*theIo);
    myTimer->expires_after(std::chrono::milliseconds(
        static_cast<long>(myTimeoutSecs * 1000)));
    auto& myStore = theStorage.lists;
    myTimer->async_wait(
        [myId, mySrcCopy, mySink, &myStore, myTimer](
            const boost::system::error_code& aEc) {
          if (aEc == boost::asio::error::operation_aborted) {
            return; // wake fired first
          }
          if (myStore.cancelWaiter(mySrcCopy, myId)) {
            mySink(RespParser::formatNullBulkString(), nullptr);
          }
        });
  }

  return std::nullopt; // suspended
}

namespace {

// Shared first-wake-wins coordination state for BLMPOP. Multi-key
// blocking pops register one waiter per key; whichever fires first
// (producer wake on any key, or the global timeout) cancels the
// rest and delivers the reply. Late wakes / inline pops that lose
// the CAS push their value back to its source key so no data is
// silently lost.
struct BlmpopState {
  std::atomic<bool>                                          fired{false};
  std::mutex                                                 mu;
  std::vector<std::pair<std::string, okts::stor::WaiterId>>  regs;
  // Held so the timer object outlives any in-flight async_wait. Set
  // before the timer is armed; nullptr when there's no timeout.
  std::shared_ptr<boost::asio::steady_timer>                 timer;
};

} // namespace

// Wire format:
//   BLMPOP timeout numkeys key [key ...] LEFT|RIGHT [COUNT count]
//
// Try-immediate pass mirrors LMPOP: walk the keys in order and pop
// from the first non-empty one. If none has data, register one
// waiter per key on a shared BlmpopState; the first wake (producer
// or timeout) wins via CAS, cancels the others, and replies. The
// loser of the CAS path pushes its transferred value back so no
// data is dropped.
std::optional<std::string>
RespHandler::handleBlmpop(const Args& aArgs) {
  // Minimum: BLMPOP timeout numkeys key1 LEFT|RIGHT  -> 5 args.
  auto myErr = validateMinArgs(aArgs, 5, "blmpop");
  if (!myErr.empty()) return myErr;

  double myTimeoutSecs = 0.0;
  try {
    size_t myConsumed = 0;
    myTimeoutSecs = std::stod(aArgs[1], &myConsumed);
    if (myConsumed != aArgs[1].size()) {
      throw std::invalid_argument("trailing garbage");
    }
  } catch (...) {
    return RespParser::formatError("ERR timeout is not a float or out of range");
  }
  if (myTimeoutSecs < 0) {
    return RespParser::formatError("ERR timeout is negative");
  }

  auto myNumKeysOpt = parseUnsignedInt(aArgs[2]);
  if (!myNumKeysOpt) {
    return RespParser::formatError(std::string(kErrNotInt));
  }
  // Bound numKeys: same OOB-arithmetic guard as handleLmpop.
  if (*myNumKeysOpt == 0 || *myNumKeysOpt > aArgs.size() ||
      *myNumKeysOpt > aArgs.size() - 4) {
    return RespParser::formatError("ERR syntax error");
  }
  const uint64_t myNumKeys = *myNumKeysOpt;

  auto myDirection = parseDirection(aArgs[3 + myNumKeys]);
  if (!myDirection) {
    return RespParser::formatError("ERR syntax error");
  }
  const bool myFront = (*myDirection == stor::Lists::Direction::LEFT);

  uint64_t myCount = 1;
  size_t   myIdx   = 4 + myNumKeys;
  if (myIdx + 1 < aArgs.size() && iequalsToUpper(aArgs[myIdx], "COUNT")) {
    auto myParsed = parseUnsignedInt(aArgs[myIdx + 1]);
    if (!myParsed || *myParsed == 0) {
      return RespParser::formatError(std::string(kErrNotInt));
    }
    myCount = *myParsed;
    myIdx += 2;
  }
  if (myIdx != aArgs.size()) {
    return RespParser::formatError("ERR syntax error");
  }

  // Try-immediate pass: walk keys in order, return on the first hit.
  for (uint64_t i = 0; i < myNumKeys; ++i) {
    const auto& myKey = aArgs[3 + i];
    auto        myValues = myFront
                               ? theStorage.lists.popFront(myKey, myCount)
                               : theStorage.lists.popBack(myKey, myCount);
    if (!myValues.empty()) {
      return formatLmpopReply(myKey, myValues);
    }
  }

  // All empty — block. Need an async context.
  if (theIo == nullptr || !theSink) {
    return RespParser::formatError(
        "ERR blocking commands require an async connection context");
  }

  auto myState     = std::make_shared<BlmpopState>();
  auto mySink      = theSink;
  auto myStorePtr  = &theStorage.lists;

  // Build the per-key wake closure. Captured-by-value so each waiter
  // owns its key. Shared state goes through `myState` (shared_ptr).
  auto myMakeWake = [myState, myStorePtr, mySink, myFront, myCount](
                        std::string aKey) {
    return [myState, myStorePtr, mySink, myFront, myCount,
            aKey = std::move(aKey)](std::optional<std::string> aValue) {
      bool myExpected = false;
      if (!myState->fired.compare_exchange_strong(myExpected, true,
                                                  std::memory_order_acq_rel)) {
        // Late wake. Recovery: if the producer transferred a value to
        // us, push it back so no data is lost. The actual winner has
        // already (or will shortly) deliver its own reply.
        if (aValue) {
          std::vector<std::string_view> myV{*aValue};
          if (myFront) myStorePtr->pushFront(aKey, myV);
          else         myStorePtr->pushBack(aKey, myV);
        }
        return;
      }

      // Won the race. Cancel the timer and any sibling waiters.
      // Self-cancel is a no-op (the waiter's already drained off the
      // shard's waiter list by the producer), so we don't bother
      // skipping our own id.
      if (myState->timer) {
        boost::system::error_code myEc;
        myState->timer->cancel(myEc);
      }
      {
        std::lock_guard<std::mutex> myLock(myState->mu);
        for (auto& myReg : myState->regs) {
          myStorePtr->cancelWaiter(myReg.first, myReg.second);
        }
      }

      if (!aValue) {
        mySink(formatBlpopNilReply(), nullptr);
        return;
      }

      // Got one value from the producer drain. Opportunistically pop
      // up to COUNT-1 more from the same key (LMPOP-style).
      std::vector<std::string> myPopped;
      myPopped.reserve(myCount);
      myPopped.emplace_back(std::move(*aValue));
      if (myCount > 1) {
        auto myExtras = myFront
                            ? myStorePtr->popFront(aKey, myCount - 1)
                            : myStorePtr->popBack(aKey, myCount - 1);
        for (auto& myV : myExtras) {
          myPopped.emplace_back(std::move(myV));
        }
      }

      // Recovery: if the connection is gone by the time the dispatched
      // lambda runs, push every popped value back so the BLMPOP
      // disconnect doesn't silently swallow data.
      auto myRecoveryKey    = aKey;
      auto myRecoveryValues = myPopped;
      auto myOnDead = [myStorePtr, myRecoveryKey,
                       myRecoveryValues, myFront]() {
        std::vector<std::string_view> myV;
        myV.reserve(myRecoveryValues.size());
        for (const auto& myS : myRecoveryValues) {
          myV.emplace_back(myS);
        }
        if (myFront) myStorePtr->pushFront(myRecoveryKey, myV);
        else         myStorePtr->pushBack(myRecoveryKey, myV);
      };

      mySink(formatLmpopReply(aKey, myPopped), std::move(myOnDead));
    };
  };

  // Registration loop. A wake on an earlier key can fire while we're
  // still registering later ones — the BlmpopState CAS handles that
  // case (the wake wins, cancel-others races with our late record,
  // and the post-tryPop "fired?" check self-cancels any waiter we
  // managed to add after the wake fired).
  for (uint64_t i = 0; i < myNumKeys; ++i) {
    if (myState->fired.load(std::memory_order_acquire)) {
      // A wake on an earlier key already won. Don't register more.
      break;
    }

    const std::string& myKey = aArgs[3 + i];
    okts::stor::WaiterId myId = 0;
    auto myImmediate = myFront
        ? theStorage.lists.tryPopFrontOrWait(myKey, myMakeWake(myKey), &myId)
        : theStorage.lists.tryPopBackOrWait(myKey, myMakeWake(myKey), &myId);

    if (myImmediate) {
      // Producer pushed to this key between the try-immediate pass
      // above and tryPopOrWait. Try to be the wake winner.
      bool myExpected = false;
      if (!myState->fired.compare_exchange_strong(myExpected, true,
                                                  std::memory_order_acq_rel)) {
        // A wake on an earlier key beat us. Push our value back; the
        // earlier wake delivers the actual reply.
        std::vector<std::string_view> myV{*myImmediate};
        if (myFront) theStorage.lists.pushFront(myKey, myV);
        else         theStorage.lists.pushBack(myKey, myV);
        return std::nullopt; // suspended (the wake winner replies)
      }

      // We won inline. Cancel any waiters we registered on prior keys.
      {
        std::lock_guard<std::mutex> myLock(myState->mu);
        for (auto& myReg : myState->regs) {
          theStorage.lists.cancelWaiter(myReg.first, myReg.second);
        }
      }

      std::vector<std::string> myPopped;
      myPopped.reserve(myCount);
      myPopped.emplace_back(std::move(*myImmediate));
      if (myCount > 1) {
        auto myExtras = myFront
                            ? theStorage.lists.popFront(myKey, myCount - 1)
                            : theStorage.lists.popBack(myKey, myCount - 1);
        for (auto& myV : myExtras) {
          myPopped.emplace_back(std::move(myV));
        }
      }
      return formatLmpopReply(myKey, myPopped);
    }

    // Registered. Record the id under the state lock; if a wake fired
    // between tryPop returning and this record, self-cancel.
    std::lock_guard<std::mutex> myLock(myState->mu);
    if (myState->fired.load(std::memory_order_acquire)) {
      theStorage.lists.cancelWaiter(myKey, myId);
      break;
    }
    myState->regs.emplace_back(myKey, myId);
  }

  // If a wake already won during registration, the wake closure is
  // responsible for delivering the reply — we just suspend.
  if (myState->fired.load(std::memory_order_acquire)) {
    return std::nullopt;
  }

  // Arm the global timeout. Stored on the state so the wake-winner
  // can cancel it; held in `myTimer` capture so the timer object
  // outlives the async_wait if the state ref drops first.
  if (myTimeoutSecs > 0) {
    auto myTimer = std::make_shared<boost::asio::steady_timer>(*theIo);
    myTimer->expires_after(std::chrono::milliseconds(
        static_cast<long>(myTimeoutSecs * 1000)));
    {
      // Publish the timer before arming so the wake-winner sees it
      // (the wake reads myState->timer to cancel it).
      std::lock_guard<std::mutex> myLock(myState->mu);
      myState->timer = myTimer;
    }
    myTimer->async_wait(
        [myState, myStorePtr, mySink, myTimer](
            const boost::system::error_code& aEc) {
          if (aEc == boost::asio::error::operation_aborted) {
            return; // a wake beat us
          }
          bool myExpected = false;
          if (!myState->fired.compare_exchange_strong(
                  myExpected, true, std::memory_order_acq_rel)) {
            return; // a wake beat us (race with the abort check above)
          }
          {
            std::lock_guard<std::mutex> myLock(myState->mu);
            for (auto& myReg : myState->regs) {
              myStorePtr->cancelWaiter(myReg.first, myReg.second);
            }
          }
          mySink(formatBlpopNilReply(), nullptr);
        });
  }

  return std::nullopt; // suspended
}

} // namespace okts::resp
