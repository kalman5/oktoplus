#pragma once

#include "Storage/storage_context.h"
#include "Resp/resp_parser.h"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace okts::resp {

class RespHandler
{
 public:
  explicit RespHandler(stor::StorageContext& aStorage);

  std::string handle(const std::vector<std::string>& aArgs);

 private:
  using Args        = std::vector<std::string>;
  using HandlerFunc = std::function<std::string(const Args&)>;

  // Helpers
  static std::string validateMinArgs(const Args& aArgs,
                                     size_t aMin,
                                     std::string_view aCommand);
  static std::vector<std::string_view> extractValues(const Args& aArgs,
                                                     size_t aFrom);
  template <typename Container>
  static std::string formatBulkStringArray(const Container& aValues) {
    std::vector<std::string> myFormatted;
    myFormatted.reserve(aValues.size());
    for (const auto& myVal : aValues) {
      myFormatted.push_back(RespParser::formatBulkString(myVal));
    }
    return RespParser::formatArray(myFormatted);
  }

  // Generic push: parameterized by storage method
  using PushMethod = size_t (stor::Lists::*)(
      const std::string&, const std::vector<std::string_view>&);
  std::string handlePush(const Args& aArgs,
                         std::string_view aCommand,
                         PushMethod aMethod);

  // Generic pop with optional count arg (single vs multi return)
  template <typename PopFunc>
  std::string handlePopWithOptionalCount(const Args& aArgs,
                                         std::string_view aCommand,
                                         PopFunc&& aPopFunc);

  // Generic multi-key set operation -> bulk string array
  template <typename SetOpFunc>
  std::string handleSetOp(const Args& aArgs,
                          std::string_view aCommand,
                          size_t aKeysFrom,
                          SetOpFunc&& aFunc);

  // General
  std::string handlePing(const Args& aArgs);
  std::string handleQuit(const Args& aArgs);
  std::string handleCommand(const Args& aArgs);
  std::string handleClient(const Args& aArgs);
  std::string handleSelect(const Args& aArgs);
  std::string handleInfo(const Args& aArgs);

  // List commands
  std::string handleLlen(const Args& aArgs);
  std::string handleLindex(const Args& aArgs);
  std::string handleLinsert(const Args& aArgs);
  std::string handleLrange(const Args& aArgs);
  std::string handleLrem(const Args& aArgs);
  std::string handleLset(const Args& aArgs);
  std::string handleLtrim(const Args& aArgs);
  std::string handleLmove(const Args& aArgs);
  std::string handleLpos(const Args& aArgs);
  std::string handleLmpop(const Args& aArgs);

  // Set commands
  std::string handleSadd(const Args& aArgs);
  std::string handleScard(const Args& aArgs);
  std::string handleSdiffstore(const Args& aArgs);
  std::string handleSintercard(const Args& aArgs);
  std::string handleSinterstore(const Args& aArgs);
  std::string handleSismember(const Args& aArgs);
  std::string handleSmismember(const Args& aArgs);
  std::string handleSmove(const Args& aArgs);
  std::string handleSrem(const Args& aArgs);
  std::string handleSunionstore(const Args& aArgs);

  stor::StorageContext&                        theStorage;
  std::unordered_map<std::string, HandlerFunc> theHandlers;
};

} // namespace okts::resp
