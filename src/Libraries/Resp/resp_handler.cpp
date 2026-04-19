#include "Resp/resp_handler.h"
#include "Resp/resp_parser.h"

#include <glog/logging.h>

#include <algorithm>
#include <cctype>

namespace okts::resp {

namespace {

std::string toUpper(std::string aStr) {
  std::transform(
      aStr.begin(), aStr.end(), aStr.begin(), [](unsigned char c) {
        return std::toupper(c);
      });
  return aStr;
}

stor::Lists::Direction parseDirection(const std::string& aDir) {
  auto myUpper = toUpper(aDir);
  if (myUpper == "LEFT") {
    return stor::Lists::Direction::LEFT;
  }
  return stor::Lists::Direction::RIGHT;
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
std::string RespHandler::handlePopWithOptionalCount(const Args& aArgs,
                                                    std::string_view aCommand,
                                                    PopFunc&& aPopFunc) {
  auto myErr = validateMinArgs(aArgs, 2, aCommand);
  if (!myErr.empty()) return myErr;

  uint64_t myCount = 1;
  bool     myMulti = false;

  if (aArgs.size() > 2) {
    myCount = std::stoull(aArgs[2]);
    myMulti = true;
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

namespace {

// Maximum command-name length we accept for stack-buffer upper-casing.
// All implemented RESP commands fit (longest is SUNIONSTORE = 11).
constexpr size_t kMaxCmdLen = 16;

// Upper-case `aIn` into `aBuf` and return a string_view over it.
// Returns std::string_view{} on overflow (caller treats as unknown).
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

} // namespace

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

    // List push (generic)
    {"LPUSH",        [](RespHandler& h, const Args& a) { return h.handlePush(a, "lpush",  &stor::Lists::pushFront); }},
    {"RPUSH",        [](RespHandler& h, const Args& a) { return h.handlePush(a, "rpush",  &stor::Lists::pushBack); }},
    {"LPUSHX",       [](RespHandler& h, const Args& a) { return h.handlePush(a, "lpushx", &stor::Lists::pushFrontExist); }},
    {"RPUSHX",       [](RespHandler& h, const Args& a) { return h.handlePush(a, "rpushx", &stor::Lists::pushBackExist); }},

    // List pop (generic)
    {"LPOP",         [](RespHandler& h, const Args& a) {
      return h.handlePopWithOptionalCount(a, "lpop", [&h](const std::string& k, uint64_t c) {
        return h.theStorage.lists.popFront(k, c);
      });
    }},
    {"RPOP",         [](RespHandler& h, const Args& a) {
      return h.handlePopWithOptionalCount(a, "rpop", [&h](const std::string& k, uint64_t c) {
        return h.theStorage.lists.popBack(k, c);
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
      return h.handlePopWithOptionalCount(a, "spop", [&h](const std::string& k, uint64_t c) {
        return h.theStorage.sets.pop(k, static_cast<size_t>(c));
      });
    }},
    {"SRANDMEMBER",  [](RespHandler& h, const Args& a) {
      return h.handlePopWithOptionalCount(a, "srandmember", [&h](const std::string& k, uint64_t c) {
        return h.theStorage.sets.randMember(k, static_cast<int64_t>(c));
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

  char            myBuf[kMaxCmdLen];
  std::string_view myCmd = toUpperStack(aArgs[0], myBuf);

  if (!myCmd.empty()) {
    for (const auto& myEntry : kHandlers) {
      if (myEntry.name == myCmd) {
        try {
          return myEntry.fn(*this, aArgs);
        } catch (const std::exception& e) {
          return RespParser::formatError(std::string("ERR ") + e.what());
        }
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
  return RespParser::formatSimpleString("OK");
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

  auto myResult = theStorage.lists.index(aArgs[1], std::stoll(aArgs[2]));
  if (myResult) {
    return RespParser::formatBulkString(myResult.value());
  }
  return RespParser::formatNullBulkString();
}

std::string RespHandler::handleLinsert(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 5, "linsert");
  if (!myErr.empty()) return myErr;

  auto myPosStr = toUpper(aArgs[2]);

  stor::Lists::Position myPosition;
  if (myPosStr == "BEFORE") {
    myPosition = stor::Lists::Position::BEFORE;
  } else if (myPosStr == "AFTER") {
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

  auto myValues =
      theStorage.lists.range(aArgs[1], std::stoll(aArgs[2]), std::stoll(aArgs[3]));
  return formatBulkStringArray(myValues);
}

std::string RespHandler::handleLrem(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 4, "lrem");
  if (!myErr.empty()) return myErr;

  auto myRemoved =
      theStorage.lists.remove(aArgs[1], std::stoll(aArgs[2]), aArgs[3]);
  return RespParser::formatInteger(static_cast<int64_t>(myRemoved));
}

std::string RespHandler::handleLset(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 4, "lset");
  if (!myErr.empty()) return myErr;

  auto myStatus =
      theStorage.lists.set(aArgs[1], std::stoll(aArgs[2]), aArgs[3]);
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

  theStorage.lists.trim(aArgs[1], std::stoll(aArgs[2]), std::stoll(aArgs[3]));
  return RespParser::formatSimpleString("OK");
}

std::string RespHandler::handleLmove(const Args& aArgs) {
  auto myErr = validateMinArgs(aArgs, 5, "lmove");
  if (!myErr.empty()) return myErr;

  auto mySrcDir  = parseDirection(aArgs[3]);
  auto myDestDir = parseDirection(aArgs[4]);

  auto myResult =
      theStorage.lists.move(aArgs[1], aArgs[2], mySrcDir, myDestDir);
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
    auto myOpt = toUpper(aArgs[i]);
    if (myOpt == "RANK") {
      myRank = std::stoll(aArgs[i + 1]);
    } else if (myOpt == "COUNT") {
      myCount = std::stoull(aArgs[i + 1]);
      myMulti = true;
    } else if (myOpt == "MAXLEN") {
      myMaxLen = std::stoull(aArgs[i + 1]);
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

  const uint64_t myNumKeys = std::stoull(aArgs[1]);
  if (aArgs.size() < 2 + myNumKeys + 1) {
    return RespParser::formatError("ERR syntax error");
  }

  auto myDirection = parseDirection(aArgs[2 + myNumKeys]);

  uint64_t myCount = 1;
  size_t   myIdx   = 3 + myNumKeys;
  if (myIdx + 1 < aArgs.size() && toUpper(aArgs[myIdx]) == "COUNT") {
    myCount = std::stoull(aArgs[myIdx + 1]);
  }

  // LMPOP semantics: return the first key that yielded values, plus its
  // popped values, as a 2-element array [key, [values]]. Null when none.
  for (uint64_t i = 0; i < myNumKeys; ++i) {
    const auto& myKey = aArgs[2 + i];
    auto        myValues =
        myDirection == stor::Lists::Direction::LEFT
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

  const uint64_t myNumKeys = std::stoull(aArgs[1]);
  if (aArgs.size() < 2 + myNumKeys) {
    return RespParser::formatError("ERR syntax error");
  }

  std::vector<std::string_view> myKeys;
  myKeys.reserve(myNumKeys);
  for (uint64_t i = 0; i < myNumKeys; ++i) {
    myKeys.emplace_back(aArgs[2 + i]);
  }

  uint64_t myLimit = 0;
  size_t   myIdx   = 2 + myNumKeys;
  if (myIdx + 1 < aArgs.size() && toUpper(aArgs[myIdx]) == "LIMIT") {
    myLimit = std::stoull(aArgs[myIdx + 1]);
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
