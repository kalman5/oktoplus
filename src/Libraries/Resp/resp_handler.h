#pragma once

#include "Storage/storage_context.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace okts::resp {

class RespHandler
{
 public:
  explicit RespHandler(stor::StorageContext& aStorage);

  std::string handle(const std::vector<std::string>& aArgs);

 private:
  using HandlerFunc =
      std::function<std::string(const std::vector<std::string>&)>;

  // General
  std::string handlePing(const std::vector<std::string>& aArgs);
  std::string handleQuit(const std::vector<std::string>& aArgs);
  std::string handleCommand(const std::vector<std::string>& aArgs);
  std::string handleClient(const std::vector<std::string>& aArgs);
  std::string handleSelect(const std::vector<std::string>& aArgs);
  std::string handleInfo(const std::vector<std::string>& aArgs);

  // List commands
  std::string handleLpush(const std::vector<std::string>& aArgs);
  std::string handleRpush(const std::vector<std::string>& aArgs);
  std::string handleLpop(const std::vector<std::string>& aArgs);
  std::string handleRpop(const std::vector<std::string>& aArgs);
  std::string handleLlen(const std::vector<std::string>& aArgs);
  std::string handleLindex(const std::vector<std::string>& aArgs);
  std::string handleLinsert(const std::vector<std::string>& aArgs);
  std::string handleLrange(const std::vector<std::string>& aArgs);
  std::string handleLrem(const std::vector<std::string>& aArgs);
  std::string handleLset(const std::vector<std::string>& aArgs);
  std::string handleLtrim(const std::vector<std::string>& aArgs);
  std::string handleLpushx(const std::vector<std::string>& aArgs);
  std::string handleRpushx(const std::vector<std::string>& aArgs);
  std::string handleLmove(const std::vector<std::string>& aArgs);
  std::string handleLpos(const std::vector<std::string>& aArgs);
  std::string handleLmpop(const std::vector<std::string>& aArgs);

  // Set commands
  std::string handleSadd(const std::vector<std::string>& aArgs);
  std::string handleScard(const std::vector<std::string>& aArgs);
  std::string handleSdiff(const std::vector<std::string>& aArgs);
  std::string handleSdiffstore(const std::vector<std::string>& aArgs);
  std::string handleSinter(const std::vector<std::string>& aArgs);
  std::string handleSintercard(const std::vector<std::string>& aArgs);
  std::string handleSinterstore(const std::vector<std::string>& aArgs);
  std::string handleSismember(const std::vector<std::string>& aArgs);
  std::string handleSmismember(const std::vector<std::string>& aArgs);
  std::string handleSmembers(const std::vector<std::string>& aArgs);
  std::string handleSmove(const std::vector<std::string>& aArgs);
  std::string handleSpop(const std::vector<std::string>& aArgs);
  std::string handleSrandmember(const std::vector<std::string>& aArgs);
  std::string handleSrem(const std::vector<std::string>& aArgs);
  std::string handleSunion(const std::vector<std::string>& aArgs);
  std::string handleSunionstore(const std::vector<std::string>& aArgs);

  stor::StorageContext&                      theStorage;
  std::unordered_map<std::string, HandlerFunc> theHandlers;
};

} // namespace okts::resp
