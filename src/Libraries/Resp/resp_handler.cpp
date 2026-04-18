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

RespHandler::RespHandler(stor::StorageContext& aStorage)
    : theStorage(aStorage)
    , theHandlers() {

  using namespace std::placeholders;
  theHandlers["PING"]    = std::bind(&RespHandler::handlePing, this, _1);
  theHandlers["QUIT"]    = std::bind(&RespHandler::handleQuit, this, _1);
  theHandlers["COMMAND"] = std::bind(&RespHandler::handleCommand, this, _1);
  theHandlers["CLIENT"]  = std::bind(&RespHandler::handleClient, this, _1);
  theHandlers["SELECT"]  = std::bind(&RespHandler::handleSelect, this, _1);
  theHandlers["INFO"]    = std::bind(&RespHandler::handleInfo, this, _1);

  theHandlers["LPUSH"]   = std::bind(&RespHandler::handleLpush, this, _1);
  theHandlers["RPUSH"]   = std::bind(&RespHandler::handleRpush, this, _1);
  theHandlers["LPOP"]    = std::bind(&RespHandler::handleLpop, this, _1);
  theHandlers["RPOP"]    = std::bind(&RespHandler::handleRpop, this, _1);
  theHandlers["LLEN"]    = std::bind(&RespHandler::handleLlen, this, _1);
  theHandlers["LINDEX"]  = std::bind(&RespHandler::handleLindex, this, _1);
  theHandlers["LINSERT"] = std::bind(&RespHandler::handleLinsert, this, _1);
  theHandlers["LRANGE"]  = std::bind(&RespHandler::handleLrange, this, _1);
  theHandlers["LREM"]    = std::bind(&RespHandler::handleLrem, this, _1);
  theHandlers["LSET"]    = std::bind(&RespHandler::handleLset, this, _1);
  theHandlers["LTRIM"]   = std::bind(&RespHandler::handleLtrim, this, _1);
  theHandlers["LPUSHX"]  = std::bind(&RespHandler::handleLpushx, this, _1);
  theHandlers["RPUSHX"]  = std::bind(&RespHandler::handleRpushx, this, _1);
  theHandlers["LMOVE"]   = std::bind(&RespHandler::handleLmove, this, _1);
  theHandlers["LPOS"]    = std::bind(&RespHandler::handleLpos, this, _1);
  theHandlers["LMPOP"]   = std::bind(&RespHandler::handleLmpop, this, _1);

  theHandlers["SADD"]    = std::bind(&RespHandler::handleSadd, this, _1);
  theHandlers["SCARD"]   = std::bind(&RespHandler::handleScard, this, _1);
  theHandlers["SDIFF"]   = std::bind(&RespHandler::handleSdiff, this, _1);
}

std::string RespHandler::handle(const std::vector<std::string>& aArgs) {
  if (aArgs.empty()) {
    return RespParser::formatError("ERR empty command");
  }

  auto myCommand = toUpper(aArgs[0]);

  auto myIt = theHandlers.find(myCommand);
  if (myIt == theHandlers.end()) {
    return RespParser::formatError(
        "ERR unknown command '" + aArgs[0] + "'");
  }

  try {
    return myIt->second(aArgs);
  } catch (const std::exception& e) {
    return RespParser::formatError(std::string("ERR ") + e.what());
  }
}

// ---- General commands ----

std::string RespHandler::handlePing(const std::vector<std::string>& aArgs) {
  if (aArgs.size() > 1) {
    return RespParser::formatBulkString(aArgs[1]);
  }
  return RespParser::formatSimpleString("PONG");
}

std::string RespHandler::handleQuit(const std::vector<std::string>&) {
  return RespParser::formatSimpleString("OK");
}

std::string RespHandler::handleCommand(const std::vector<std::string>&) {
  return RespParser::formatEmptyArray();
}

std::string RespHandler::handleClient(const std::vector<std::string>&) {
  return RespParser::formatSimpleString("OK");
}

std::string RespHandler::handleSelect(const std::vector<std::string>&) {
  return RespParser::formatSimpleString("OK");
}

std::string RespHandler::handleInfo(const std::vector<std::string>&) {
  return RespParser::formatBulkString(
      "# Server\r\noktoplus_version:0.1.0\r\n");
}

// ---- List commands ----

std::string RespHandler::handleLpush(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 3) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'lpush' command");
  }
  const auto& myKey = aArgs[1];
  std::vector<std::string_view> myValues;
  myValues.reserve(aArgs.size() - 2);
  for (size_t i = 2; i < aArgs.size(); ++i) {
    myValues.emplace_back(aArgs[i]);
  }
  auto mySize = theStorage.lists.pushFront(myKey, myValues);
  return RespParser::formatInteger(static_cast<int64_t>(mySize));
}

std::string RespHandler::handleRpush(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 3) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'rpush' command");
  }
  const auto& myKey = aArgs[1];
  std::vector<std::string_view> myValues;
  myValues.reserve(aArgs.size() - 2);
  for (size_t i = 2; i < aArgs.size(); ++i) {
    myValues.emplace_back(aArgs[i]);
  }
  auto mySize = theStorage.lists.pushBack(myKey, myValues);
  return RespParser::formatInteger(static_cast<int64_t>(mySize));
}

std::string RespHandler::handleLpop(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 2) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'lpop' command");
  }
  const auto& myKey   = aArgs[1];
  uint64_t    myCount = 1;
  bool        myMulti = false;

  if (aArgs.size() > 2) {
    myCount = std::stoull(aArgs[2]);
    myMulti = true;
  }

  auto myValues = theStorage.lists.popFront(myKey, myCount);

  if (myMulti) {
    std::vector<std::string> myFormatted;
    for (const auto& myVal : myValues) {
      myFormatted.push_back(RespParser::formatBulkString(myVal));
    }
    return RespParser::formatArray(myFormatted);
  }

  if (myValues.empty()) {
    return RespParser::formatNullBulkString();
  }
  return RespParser::formatBulkString(myValues.front());
}

std::string RespHandler::handleRpop(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 2) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'rpop' command");
  }
  const auto& myKey   = aArgs[1];
  uint64_t    myCount = 1;
  bool        myMulti = false;

  if (aArgs.size() > 2) {
    myCount = std::stoull(aArgs[2]);
    myMulti = true;
  }

  auto myValues = theStorage.lists.popBack(myKey, myCount);

  if (myMulti) {
    std::vector<std::string> myFormatted;
    for (const auto& myVal : myValues) {
      myFormatted.push_back(RespParser::formatBulkString(myVal));
    }
    return RespParser::formatArray(myFormatted);
  }

  if (myValues.empty()) {
    return RespParser::formatNullBulkString();
  }
  return RespParser::formatBulkString(myValues.front());
}

std::string RespHandler::handleLlen(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 2) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'llen' command");
  }
  auto mySize = theStorage.lists.size(aArgs[1]);
  return RespParser::formatInteger(static_cast<int64_t>(mySize));
}

std::string RespHandler::handleLindex(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 3) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'lindex' command");
  }
  auto myResult = theStorage.lists.index(aArgs[1], std::stoll(aArgs[2]));
  if (myResult) {
    return RespParser::formatBulkString(myResult.value());
  }
  return RespParser::formatNullBulkString();
}

std::string RespHandler::handleLinsert(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 5) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'linsert' command");
  }
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

std::string RespHandler::handleLrange(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 4) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'lrange' command");
  }
  auto myValues =
      theStorage.lists.range(aArgs[1], std::stoll(aArgs[2]), std::stoll(aArgs[3]));

  std::vector<std::string> myFormatted;
  myFormatted.reserve(myValues.size());
  for (const auto& myVal : myValues) {
    myFormatted.push_back(RespParser::formatBulkString(myVal));
  }
  return RespParser::formatArray(myFormatted);
}

std::string RespHandler::handleLrem(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 4) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'lrem' command");
  }
  auto myRemoved =
      theStorage.lists.remove(aArgs[1], std::stoll(aArgs[2]), aArgs[3]);
  return RespParser::formatInteger(static_cast<int64_t>(myRemoved));
}

std::string RespHandler::handleLset(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 4) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'lset' command");
  }
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

std::string RespHandler::handleLtrim(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 4) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'ltrim' command");
  }
  theStorage.lists.trim(aArgs[1], std::stoll(aArgs[2]), std::stoll(aArgs[3]));
  return RespParser::formatSimpleString("OK");
}

std::string RespHandler::handleLpushx(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 3) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'lpushx' command");
  }
  const auto& myKey = aArgs[1];
  std::vector<std::string_view> myValues;
  myValues.reserve(aArgs.size() - 2);
  for (size_t i = 2; i < aArgs.size(); ++i) {
    myValues.emplace_back(aArgs[i]);
  }
  auto mySize = theStorage.lists.pushFrontExist(myKey, myValues);
  return RespParser::formatInteger(static_cast<int64_t>(mySize));
}

std::string RespHandler::handleRpushx(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 3) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'rpushx' command");
  }
  const auto& myKey = aArgs[1];
  std::vector<std::string_view> myValues;
  myValues.reserve(aArgs.size() - 2);
  for (size_t i = 2; i < aArgs.size(); ++i) {
    myValues.emplace_back(aArgs[i]);
  }
  auto mySize = theStorage.lists.pushBackExist(myKey, myValues);
  return RespParser::formatInteger(static_cast<int64_t>(mySize));
}

std::string RespHandler::handleLmove(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 5) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'lmove' command");
  }
  auto mySrcDir  = parseDirection(aArgs[3]);
  auto myDestDir = parseDirection(aArgs[4]);

  auto myResult =
      theStorage.lists.move(aArgs[1], aArgs[2], mySrcDir, myDestDir);
  if (myResult) {
    return RespParser::formatBulkString(myResult.value());
  }
  return RespParser::formatNullBulkString();
}

std::string RespHandler::handleLpos(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 3) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'lpos' command");
  }
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

std::string RespHandler::handleLmpop(const std::vector<std::string>& aArgs) {
  // LMPOP numkeys key [key ...] LEFT|RIGHT [COUNT count]
  if (aArgs.size() < 4) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'lmpop' command");
  }

  const uint64_t myNumKeys = std::stoull(aArgs[1]);
  if (aArgs.size() < 2 + myNumKeys + 1) {
    return RespParser::formatError("ERR syntax error");
  }

  std::vector<std::string> myKeys;
  myKeys.reserve(myNumKeys);
  for (uint64_t i = 0; i < myNumKeys; ++i) {
    myKeys.push_back(aArgs[2 + i]);
  }

  auto myDirection = parseDirection(aArgs[2 + myNumKeys]);

  uint64_t myCount = 1;
  size_t   myIdx   = 3 + myNumKeys;
  if (myIdx + 1 < aArgs.size() && toUpper(aArgs[myIdx]) == "COUNT") {
    myCount = std::stoull(aArgs[myIdx + 1]);
  }

  auto myValues = theStorage.lists.multiplePop(myKeys, myDirection, myCount);

  if (myValues.empty()) {
    return RespParser::formatNullBulkString();
  }

  std::vector<std::string> myFormatted;
  for (const auto& myVal : myValues) {
    myFormatted.push_back(RespParser::formatBulkString(myVal));
  }
  return RespParser::formatArray(myFormatted);
}

// ---- Set commands ----

std::string RespHandler::handleSadd(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 3) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'sadd' command");
  }
  const auto& myKey = aArgs[1];
  std::vector<std::string_view> myValues;
  myValues.reserve(aArgs.size() - 2);
  for (size_t i = 2; i < aArgs.size(); ++i) {
    myValues.emplace_back(aArgs[i]);
  }
  auto mySize = theStorage.sets.add(myKey, myValues);
  return RespParser::formatInteger(static_cast<int64_t>(mySize));
}

std::string RespHandler::handleScard(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 2) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'scard' command");
  }
  auto mySize = theStorage.sets.cardinality(aArgs[1]);
  return RespParser::formatInteger(static_cast<int64_t>(mySize));
}

std::string RespHandler::handleSdiff(const std::vector<std::string>& aArgs) {
  if (aArgs.size() < 2) {
    return RespParser::formatError(
        "ERR wrong number of arguments for 'sdiff' command");
  }
  std::vector<std::string_view> myKeys;
  myKeys.reserve(aArgs.size() - 1);
  for (size_t i = 1; i < aArgs.size(); ++i) {
    myKeys.emplace_back(aArgs[i]);
  }
  auto myResult = theStorage.sets.diff(myKeys);

  std::vector<std::string> myFormatted;
  myFormatted.reserve(myResult.size());
  for (const auto& myVal : myResult) {
    myFormatted.push_back(RespParser::formatBulkString(myVal));
  }
  return RespParser::formatArray(myFormatted);
}

} // namespace okts::resp
